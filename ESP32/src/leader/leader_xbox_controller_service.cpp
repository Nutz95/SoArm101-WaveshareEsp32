#include "leader_xbox_controller_service.h"

#include "../Config/controller_mapping_config.h"
#include "../Config/leader_runtime_config.h"
#include "leader_radio_coexistence.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <vector>

namespace soarm {

namespace {
constexpr uint16_t kHidServiceUuid = 0x1812;
constexpr uint16_t kHidInputReportUuid = 0x2A4D;

LeaderXboxControllerService *g_service = nullptr;
NimBLEClient *g_client = nullptr;
NimBLEAdvertisedDevice *g_targetDevice = nullptr;

XboxClientCallbacks g_clientCallbacks;
XboxAdvertisedCallbacks g_advCallbacks;

void inputReportCallback(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify) {
  (void)characteristic;
  (void)isNotify;

  if (g_service != nullptr) {
    g_service->updateInputState(data, length);
    g_service->markInputReport();
    g_service->setRuntimeState(XboxRuntimeState::Connected);
  }
}

} // namespace

void XboxClientCallbacks::onConnect(NimBLEClient *client) {
  if (client != nullptr) {
    client->updateConnParams(12, 24, 0, 60);
  }
  if (g_service != nullptr) {
    Serial.println("[XBOX] BLE connected");
  }
}

void XboxClientCallbacks::onDisconnect(NimBLEClient *client) {
  (void)client;
  if (g_service != nullptr) {
    Serial.println("[XBOX] BLE disconnected");
    g_service->setRuntimeState(XboxRuntimeState::Disconnected);
  }
}

void XboxClientCallbacks::onAuthenticationComplete(ble_gap_conn_desc *desc) {
  if (g_service == nullptr) {
    return;
  }

  if (desc != nullptr && desc->sec_state.encrypted) {
    Serial.println("[XBOX] BLE link encrypted");
    g_service->setRuntimeState(XboxRuntimeState::Pairing);
    return;
  }

  Serial.println("[XBOX] BLE auth failed (not encrypted)");
  g_service->setRuntimeState(XboxRuntimeState::Disconnected);
}

void XboxAdvertisedCallbacks::onResult(NimBLEAdvertisedDevice *device) {
  if (device == nullptr || g_targetDevice != nullptr || g_service == nullptr) {
    return;
  }

  const std::string name = device->getName();
  const bool isHid = device->isAdvertisingService(NimBLEUUID(kHidServiceUuid));
  const bool isPreferred = !name.empty() &&
                           (name.find(config::controller::kPreferredControllerName) != std::string::npos);

  if (!isHid && !isPreferred) {
    return;
  }

  g_targetDevice = device;
  g_service->onDeviceFound(name.c_str());
  Serial.printf("[XBOX] candidate found: %s\n", name.empty() ? "(unnamed)" : name.c_str());
  NimBLEDevice::getScan()->stop();
}

void LeaderXboxControllerService::begin() {
  if (started_) {
    return;
  }

  g_service = this;
  controllerName_[0] = '\0';

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  NimBLEDevice::init("");
  applyLeaderRadioCoexistencePreference();
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  // MITM disabled because ESP has no IO capability for PIN entry/confirmation.
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  NimBLEScan *scan = NimBLEDevice::getScan();
  if (scan != nullptr) {
    scan->setAdvertisedDeviceCallbacks(&g_advCallbacks, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
  }

  TaskHandle_t taskHandle = nullptr;
  const BaseType_t created = xTaskCreatePinnedToCore(
      &LeaderXboxControllerService::taskEntry,
      "xbox_ble",
      8192,
      this,
      1,
      &taskHandle,
      1);

  started_ = (created == pdPASS);
  if (!started_) {
    g_service->setRuntimeState(XboxRuntimeState::Disconnected);
  }
}

void LeaderXboxControllerService::tick() {
  if (!started_) {
    return;
  }

  if (g_client == nullptr || !g_client->isConnected()) {
    controllerPaired_.store(false);
    inputSubscribed_.store(false);
    linkEncrypted_.store(false);
    setRuntimeState(XboxRuntimeState::Disconnected);
    return;
  }

  controllerPaired_.store(isReadyForTeleop());

  const uint32_t lastInputMs = lastInputReportMs_.load();
  const uint32_t nowMs = millis();
  const uint32_t ageMs = (lastInputMs == 0U || nowMs < lastInputMs) ? 0U : (nowMs - lastInputMs);
  if (!isReadyForTeleop() || ageMs > config::leader::kXboxInputReportStaleMs) {
    setRuntimeState(XboxRuntimeState::Pairing);
  } else {
    setRuntimeState(XboxRuntimeState::Connected);
  }
}

bool LeaderXboxControllerService::isControllerPaired() const {
  return controllerPaired_.load();
}

void LeaderXboxControllerService::setBackgroundReconnectDeferred(bool deferred) {
  backgroundReconnectDeferred_.store(deferred);
}

void LeaderXboxControllerService::snapshot(XboxRuntimeSnapshot &out) const {
  out.state = runtimeState_.load();
  out.controllerPaired = controllerPaired_.load();
  out.linkEncrypted = linkEncrypted_.load();
  out.inputSubscribed = inputSubscribed_.load();
  out.reportCount = static_cast<uint16_t>(reportCount_.load() & 0xFFFFU);
  out.buttonsMask = buttonsMask_.load();
  out.axisLeftX = axisLeftX_.load();
  out.axisLeftY = axisLeftY_.load();
  out.axisRightX = axisRightX_.load();
  out.axisRightY = axisRightY_.load();
  out.dpadX = dpadX_.load();
  out.dpadY = dpadY_.load();
  out.triggerLeft = triggerLeft_.load();
  out.triggerRight = triggerRight_.load();

  const uint32_t nowMs = millis();
  const uint32_t lastInputMs = lastInputReportMs_.load();
  if (lastInputMs == 0U || nowMs < lastInputMs) {
    out.lastReportAgeMs = 0U;
  } else {
    const uint32_t age = nowMs - lastInputMs;
    out.lastReportAgeMs = static_cast<uint16_t>(age > 65535U ? 65535U : age);
  }

  strncpy(out.controllerName, controllerName_, sizeof(out.controllerName) - 1U);
  out.controllerName[sizeof(out.controllerName) - 1U] = '\0';
}

void LeaderXboxControllerService::runLoop() {
  while (true) {
    if (g_client != nullptr && g_client->isConnected()) {
      if (!inputSubscribed_.load()) {
        inputSubscribed_.store(subscribeToInputReport());
      }
      vTaskDelay(pdMS_TO_TICKS(config::leader::kXboxConnectedTickDelayMs));
      continue;
    }

    if (backgroundReconnectDeferred_.load()) {
      setRuntimeState(XboxRuntimeState::Disconnected);
      vTaskDelay(pdMS_TO_TICKS(config::leader::kXboxScanDeferWhileTeleopMs));
      continue;
    }

    setRuntimeState(XboxRuntimeState::Scanning);
    g_targetDevice = nullptr;

    NimBLEScan *scan = NimBLEDevice::getScan();
    if (scan != nullptr) {
      const uint32_t scanWindowSec = config::leader::kXboxScanWindowMs / 1000U;
      scan->start(scanWindowSec == 0U ? 1U : scanWindowSec, false);
    }

    if (g_targetDevice != nullptr) {
      setRuntimeState(XboxRuntimeState::Pairing);
      const bool connected = connectToTarget();
      if (!connected) {
        controllerPaired_.store(false);
        setRuntimeState(XboxRuntimeState::Disconnected);
      }
    } else {
      controllerPaired_.store(false);
      setRuntimeState(XboxRuntimeState::Disconnected);
    }

    vTaskDelay(pdMS_TO_TICKS(config::leader::kXboxScanRetryDelayMs));
  }
}

bool LeaderXboxControllerService::connectToTarget() {
  if (g_targetDevice == nullptr) {
    return false;
  }

  if (g_client == nullptr) {
    g_client = NimBLEDevice::createClient();
    if (g_client == nullptr) {
      return false;
    }
    g_client->setClientCallbacks(&g_clientCallbacks, false);
  }

  if (!g_client->connect(g_targetDevice)) {
    Serial.println("[XBOX] connect failed");
    return false;
  }

  linkEncrypted_.store(false);
  inputSubscribed_.store(false);

  Serial.println("[XBOX] requesting secure link");
  g_client->secureConnection();
  if (!waitForSecureLink(2500U)) {
    Serial.println("[XBOX] secure link timeout");
    g_client->disconnect();
    return false;
  }

  std::string name = g_targetDevice->getName();
  if (name.empty()) {
    name = config::controller::kPreferredControllerName;
  }

  strncpy(controllerName_, name.c_str(), sizeof(controllerName_) - 1U);
  controllerName_[sizeof(controllerName_) - 1U] = '\0';

  const bool subscribed = subscribeToInputReport();
  inputSubscribed_.store(subscribed);
  if (!subscribed) {
    Serial.println("[XBOX] input report subscribe failed");
    g_client->disconnect();
    return false;
  }

  controllerPaired_.store(isReadyForTeleop());
  setRuntimeState(isReadyForTeleop() ? XboxRuntimeState::Connected : XboxRuntimeState::Pairing);
  Serial.printf("[XBOX] ready=%u encrypted=%u subscribed=%u\n",
                isReadyForTeleop() ? 1U : 0U,
                linkEncrypted_.load() ? 1U : 0U,
                inputSubscribed_.load() ? 1U : 0U);
  return isReadyForTeleop();
}

bool LeaderXboxControllerService::subscribeToInputReport() {
  if (g_client == nullptr || !g_client->isConnected()) {
    return false;
  }

  NimBLERemoteService *service = g_client->getService(NimBLEUUID(kHidServiceUuid));
  if (service == nullptr) {
    return false;
  }

  std::vector<NimBLERemoteCharacteristic *> *chars = service->getCharacteristics(true);
  if (chars == nullptr) {
    return false;
  }

  for (NimBLERemoteCharacteristic *characteristic : *chars) {
    if (characteristic == nullptr || !characteristic->canNotify()) {
      continue;
    }

    const bool isInputReport = (characteristic->getUUID() == NimBLEUUID(kHidInputReportUuid));
    if (!isInputReport) {
      continue;
    }

    if (characteristic->subscribe(true, inputReportCallback)) {
      Serial.println("[XBOX] input report subscribed");
      return true;
    }
  }

  return false;
}

void LeaderXboxControllerService::setRuntimeState(XboxRuntimeState state) {
  runtimeState_.store(static_cast<uint8_t>(state));
}

void LeaderXboxControllerService::markInputReport() {
  lastInputReportMs_.store(millis());
  reportCount_.store(reportCount_.load() + 1U);
}

void LeaderXboxControllerService::onDeviceFound(const char *name) {
  if (name == nullptr || name[0] == '\0') {
    return;
  }

  strncpy(controllerName_, name, sizeof(controllerName_) - 1U);
  controllerName_[sizeof(controllerName_) - 1U] = '\0';
}

bool LeaderXboxControllerService::waitForSecureLink(uint32_t timeoutMs) {
  const uint32_t startedAt = millis();
  while ((millis() - startedAt) < timeoutMs) {
    if (g_client == nullptr || !g_client->isConnected()) {
      return false;
    }

    NimBLEConnInfo connInfo = g_client->getConnInfo();
    if (connInfo.isEncrypted()) {
      linkEncrypted_.store(true);
      return true;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  return false;
}

bool LeaderXboxControllerService::isReadyForTeleop() const {
  return linkEncrypted_.load() && inputSubscribed_.load();
}

void LeaderXboxControllerService::setModeCycleButton(XboxLogicalButton button) {
  modeCycleButton_.store(static_cast<uint8_t>(button));
  modeCyclePressedLast_.store(false);
}

bool LeaderXboxControllerService::consumeModeCycleRequest() {
  uint8_t pending = modeCyclePendingCount_.load();
  if (pending == 0U) {
    return false;
  }

  modeCyclePendingCount_.store(static_cast<uint8_t>(pending - 1U));
  return true;
}

bool LeaderXboxControllerService::consumeButtonPress(XboxLogicalButton button) {
  std::atomic<uint8_t> *pendingCounter = nullptr;
  if (button == XboxLogicalButton::A) {
    pendingCounter = &buttonAPendingCount_;
  } else if (button == XboxLogicalButton::B) {
    pendingCounter = &buttonBPendingCount_;
  } else {
    return false;
  }

  uint8_t pending = pendingCounter->load();
  if (pending == 0U) {
    return false;
  }

  pendingCounter->store(static_cast<uint8_t>(pending - 1U));
  return true;
}

void LeaderXboxControllerService::discardPendingButtonPress(XboxLogicalButton button) {
  if (button == XboxLogicalButton::A) {
    buttonAPendingCount_.store(0U);
    return;
  }

  if (button == XboxLogicalButton::B) {
    buttonBPendingCount_.store(0U);
  }
}

void LeaderXboxControllerService::taskEntry(void *context) {
  if (context != nullptr) {
    LeaderXboxControllerService *service = static_cast<LeaderXboxControllerService *>(context);
    service->runLoop();
  }
  vTaskDelete(nullptr);
}

} // namespace soarm
