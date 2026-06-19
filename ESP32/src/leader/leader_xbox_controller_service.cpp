#include "leader_xbox_controller_service.h"
#include "leader_xbox_controller_service_internal.h"

#include "../Config/controller_mapping_config.h"
#include "../Config/leader_runtime_config.h"
#include "../common/cstring_copy.h"
#include "leader_radio_coexistence.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace soarm {

namespace {
constexpr uint16_t kHidServiceUuid = 0x1812;
} // namespace

LeaderXboxControllerService *g_xboxService = nullptr;
NimBLEClient *g_xboxClient = nullptr;
NimBLEAdvertisedDevice *g_xboxTargetDevice = nullptr;
XboxClientCallbacks g_xboxClientCallbacks;
XboxAdvertisedCallbacks g_xboxAdvCallbacks;

void xboxInputReportCallback(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify) {
  (void)characteristic;
  (void)isNotify;

  if (g_xboxService != nullptr) {
    g_xboxService->updateInputState(data, length);
    g_xboxService->markInputReport();
    g_xboxService->setRuntimeState(XboxRuntimeState::Connected);
  }
}

void XboxClientCallbacks::onConnect(NimBLEClient *client) {
  if (client != nullptr) {
    client->updateConnParams(12, 24, 0, 60);
  }
  if (g_xboxService != nullptr) {
    Serial.println("[XBOX] BLE connected");
  }
}

void XboxClientCallbacks::onDisconnect(NimBLEClient *client) {
  (void)client;
  if (g_xboxService != nullptr) {
    Serial.println("[XBOX] BLE disconnected");
    g_xboxService->setRuntimeState(XboxRuntimeState::Disconnected);
  }
  g_xboxTargetDevice = nullptr;
}

void XboxClientCallbacks::onAuthenticationComplete(ble_gap_conn_desc *desc) {
  if (g_xboxService == nullptr) {
    return;
  }

  if (desc != nullptr && desc->sec_state.encrypted) {
    Serial.println("[XBOX] BLE link encrypted");
    g_xboxService->setRuntimeState(XboxRuntimeState::Pairing);
    return;
  }

  Serial.println("[XBOX] BLE auth failed (not encrypted)");
  g_xboxService->setRuntimeState(XboxRuntimeState::Disconnected);
}

void XboxAdvertisedCallbacks::onResult(NimBLEAdvertisedDevice *device) {
  if (device == nullptr || g_xboxTargetDevice != nullptr || g_xboxService == nullptr) {
    return;
  }

  const std::string name = device->getName();
  const bool isHid = device->isAdvertisingService(NimBLEUUID(kHidServiceUuid));
  const bool isPreferred = !name.empty() &&
                           (name.find(config::controller::kPreferredControllerName) != std::string::npos);

  if (!isHid && !isPreferred) {
    return;
  }

  g_xboxTargetDevice = device;
  g_xboxService->onDeviceFound(name.c_str());
  Serial.printf("[XBOX] candidate found: %s\n", name.empty() ? "(unnamed)" : name.c_str());
  NimBLEDevice::getScan()->stop();
}

void LeaderXboxControllerService::begin() {
  if (started_) {
    return;
  }

  g_xboxService = this;
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
    scan->setAdvertisedDeviceCallbacks(&g_xboxAdvCallbacks, false);
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
    g_xboxService->setRuntimeState(XboxRuntimeState::Disconnected);
  }
}

void LeaderXboxControllerService::tick() {
  if (!started_) {
    return;
  }

  if (g_xboxClient == nullptr || !g_xboxClient->isConnected()) {
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

  copyCString(out.controllerName, sizeof(out.controllerName), controllerName_);
}

void LeaderXboxControllerService::runLoop() {
  while (true) {
    if (g_xboxClient != nullptr && g_xboxClient->isConnected()) {
      if (!inputSubscribed_.load()) {
        inputSubscribed_.store(subscribeToInputReport());
      }
      vTaskDelay(pdMS_TO_TICKS(config::leader::kXboxConnectedTickDelayMs));
      continue;
    }

    tearDownBleClient();
    controllerPaired_.store(false);

    // ponytail: RAM-cached BLE address only; NVS bond store if salon reconnect stays flaky.
    if (tryConnectCachedPeer()) {
      continue;
    }

    const bool teleopPriority = backgroundReconnectDeferred_.load();
    if (teleopPriority) {
      // ponytail: passive scan during teleop, upgrade to timed active scan if wake misses persist.
      if (!scanForController(true, config::leader::kXboxTeleopPassiveScanMs)) {
        setRuntimeState(XboxRuntimeState::Disconnected);
      }
      vTaskDelay(pdMS_TO_TICKS(config::leader::kXboxTeleopReconnectPollMs));
      continue;
    }

    if (!scanForController(false, config::leader::kXboxScanWindowMs)) {
      controllerPaired_.store(false);
      setRuntimeState(XboxRuntimeState::Disconnected);
    }

    vTaskDelay(pdMS_TO_TICKS(config::leader::kXboxScanRetryDelayMs));
  }
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

  copyCString(controllerName_, sizeof(controllerName_), name);
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
