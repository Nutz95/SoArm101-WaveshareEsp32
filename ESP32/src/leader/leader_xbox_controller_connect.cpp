#include "leader_xbox_controller_service.h"
#include "leader_xbox_controller_service_internal.h"

#include "../Config/controller_mapping_config.h"
#include "../Config/leader_runtime_config.h"
#include "../common/cstring_copy.h"
#include "../common/mac_address_utils.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>

namespace soarm {

namespace {
constexpr uint16_t kHidServiceUuid = 0x1812;
constexpr uint16_t kHidInputReportUuid = 0x2A4D;
} // namespace

void LeaderXboxControllerService::persistPeerAddress(const NimBLEAddress &peer) {
  const std::string addrStr = peer.toString();
  copyCString(lastPeerAddress_, sizeof(lastPeerAddress_), addrStr.c_str());

  const uint8_t *native = peer.getNative();
  if (native == nullptr) {
    return;
  }

  if (xboxAddressStore_.save(native)) {
    Serial.printf("[XBOX] stored controller address: %s\n", lastPeerAddress_);
  }
}

bool LeaderXboxControllerService::hydrateCachedAddress() {
  if (lastPeerAddress_[0] != '\0') {
    return true;
  }

  uint8_t mac[6]{};
  if (!xboxAddressStore_.load(mac)) {
    return false;
  }

  formatMacAddress(mac, lastPeerAddress_);
  return true;
}

void LeaderXboxControllerService::tearDownBleClient() {
  inputSubscribed_.store(false);
  linkEncrypted_.store(false);
  g_xboxTargetDevice = nullptr;

  if (g_xboxClient == nullptr) {
    return;
  }

  if (g_xboxClient->isConnected()) {
    g_xboxClient->disconnect();
  }
  NimBLEDevice::deleteClient(g_xboxClient);
  g_xboxClient = nullptr;
}

bool LeaderXboxControllerService::finishConnectHandshake() {
  if (g_xboxClient == nullptr || !g_xboxClient->isConnected()) {
    return false;
  }

  linkEncrypted_.store(false);
  inputSubscribed_.store(false);

  Serial.println("[XBOX] requesting secure link");
  g_xboxClient->secureConnection();
  if (!waitForSecureLink(2500U)) {
    Serial.println("[XBOX] secure link timeout");
    return false;
  }

  std::string name;
  if (g_xboxTargetDevice != nullptr) {
    name = g_xboxTargetDevice->getName();
  }
  if (name.empty()) {
    name = config::controller::kPreferredControllerName;
  }
  copyCString(controllerName_, sizeof(controllerName_), name.c_str());

  persistPeerAddress(g_xboxClient->getPeerAddress());
  bootFullScanPending_ = false;

  const bool subscribed = subscribeToInputReport();
  inputSubscribed_.store(subscribed);
  if (!subscribed) {
    Serial.println("[XBOX] input report subscribe failed");
    return false;
  }

  controllerPaired_.store(isReadyForTeleop());
  setRuntimeState(isReadyForTeleop() ? XboxRuntimeState::Connected : XboxRuntimeState::Pairing);
  Serial.printf("[XBOX] ready=%u encrypted=%u subscribed=%u addr=%s\n",
                isReadyForTeleop() ? 1U : 0U,
                linkEncrypted_.load() ? 1U : 0U,
                inputSubscribed_.load() ? 1U : 0U,
                lastPeerAddress_);
  return isReadyForTeleop();
}

bool LeaderXboxControllerService::tryConnectCachedPeer() {
  if (!hydrateCachedAddress()) {
    return false;
  }

  if (g_xboxClient == nullptr) {
    g_xboxClient = NimBLEDevice::createClient();
    if (g_xboxClient == nullptr) {
      return false;
    }
    g_xboxClient->setClientCallbacks(&g_xboxClientCallbacks, false);
  }

  setRuntimeState(XboxRuntimeState::Pairing);
  const NimBLEAddress peer(lastPeerAddress_);
  if (!g_xboxClient->connect(peer)) {
    Serial.printf("[XBOX] cached connect failed: %s\n", lastPeerAddress_);
    tearDownBleClient();
    return false;
  }

  Serial.printf("[XBOX] cached connect ok: %s\n", lastPeerAddress_);
  if (!finishConnectHandshake()) {
    tearDownBleClient();
    return false;
  }
  return true;
}

bool LeaderXboxControllerService::scanForController(bool passiveScan, uint32_t windowMs) {
  setRuntimeState(XboxRuntimeState::Scanning);
  g_xboxTargetDevice = nullptr;

  NimBLEScan *scan = NimBLEDevice::getScan();
  if (scan == nullptr) {
    return false;
  }

  scan->setActiveScan(!passiveScan);
  const uint32_t scanWindowSec = windowMs / 1000U;
  scan->start(scanWindowSec == 0U ? 1U : scanWindowSec, false);
  scan->setActiveScan(true);

  if (g_xboxTargetDevice == nullptr) {
    return false;
  }

  setRuntimeState(XboxRuntimeState::Pairing);
  return connectToTarget();
}

bool LeaderXboxControllerService::connectToTarget() {
  if (g_xboxTargetDevice == nullptr) {
    return false;
  }

  if (g_xboxClient == nullptr) {
    g_xboxClient = NimBLEDevice::createClient();
    if (g_xboxClient == nullptr) {
      return false;
    }
    g_xboxClient->setClientCallbacks(&g_xboxClientCallbacks, false);
  }

  if (!g_xboxClient->connect(g_xboxTargetDevice)) {
    Serial.println("[XBOX] connect failed");
    tearDownBleClient();
    return false;
  }

  if (!finishConnectHandshake()) {
    tearDownBleClient();
    return false;
  }
  return true;
}

bool LeaderXboxControllerService::subscribeToInputReport() {
  if (g_xboxClient == nullptr || !g_xboxClient->isConnected()) {
    return false;
  }

  NimBLERemoteService *service = g_xboxClient->getService(NimBLEUUID(kHidServiceUuid));
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

    if (characteristic->subscribe(true, xboxInputReportCallback)) {
      Serial.println("[XBOX] input report subscribed");
      return true;
    }
  }

  return false;
}

bool LeaderXboxControllerService::waitForSecureLink(uint32_t timeoutMs) {
  const uint32_t startedAt = millis();
  while ((millis() - startedAt) < timeoutMs) {
    if (g_xboxClient == nullptr || !g_xboxClient->isConnected()) {
      return false;
    }

    NimBLEConnInfo connInfo = g_xboxClient->getConnInfo();
    if (connInfo.isEncrypted()) {
      linkEncrypted_.store(true);
      return true;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  return false;
}

} // namespace soarm
