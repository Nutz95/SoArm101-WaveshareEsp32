#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <NimBLEDevice.h>

namespace soarm {

class XboxClientCallbacks : public NimBLEClientCallbacks {
public:
  void onConnect(NimBLEClient *client) override;
  void onDisconnect(NimBLEClient *client) override;
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override;
};

class XboxAdvertisedCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
  void onResult(NimBLEAdvertisedDevice *device) override;
};

enum class XboxRuntimeState : uint8_t {
  Disconnected = 0,
  Scanning = 1,
  Pairing = 2,
  Connected = 3,
};

struct XboxRuntimeSnapshot {
  uint8_t state{static_cast<uint8_t>(XboxRuntimeState::Disconnected)};
  uint16_t lastReportAgeMs{0U};
  uint16_t reportCount{0U};
  uint16_t buttonsMask{0U};
  int16_t axisLeftX{0};
  int16_t axisLeftY{0};
  int16_t axisRightX{0};
  int16_t axisRightY{0};
  uint8_t triggerLeft{0U};
  uint8_t triggerRight{0U};
  bool linkEncrypted{false};
  bool inputSubscribed{false};
  bool controllerPaired{false};
  char controllerName[32]{0};
};

class LeaderXboxControllerService {
public:
  void begin();
  void tick();
  bool isControllerPaired() const;
  void snapshot(XboxRuntimeSnapshot &out) const;
  void setRuntimeState(XboxRuntimeState state);
  void markInputReport();
  void onDeviceFound(const char *name);
  void updateInputState(const uint8_t *data, size_t length);
  bool isReadyForTeleop() const;

private:
  void runLoop();
  bool connectToTarget();
  bool subscribeToInputReport();
  bool waitForSecureLink(uint32_t timeoutMs);

  static void taskEntry(void *context);

  std::atomic<uint8_t> runtimeState_{static_cast<uint8_t>(XboxRuntimeState::Disconnected)};
  std::atomic<bool> controllerPaired_{false};
  std::atomic<bool> linkEncrypted_{false};
  std::atomic<bool> inputSubscribed_{false};
  std::atomic<uint32_t> lastInputReportMs_{0U};
  std::atomic<uint32_t> reportCount_{0U};
  std::atomic<uint16_t> buttonsMask_{0U};
  std::atomic<int16_t> axisLeftX_{0};
  std::atomic<int16_t> axisLeftY_{0};
  std::atomic<int16_t> axisRightX_{0};
  std::atomic<int16_t> axisRightY_{0};
  std::atomic<uint8_t> triggerLeft_{0U};
  std::atomic<uint8_t> triggerRight_{0U};
  char controllerName_[32]{0};
  bool started_{false};
};

} // namespace soarm
