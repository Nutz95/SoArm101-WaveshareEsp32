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

enum class XboxLogicalButton : uint8_t {
  None = 0,
  View = 1,
  Menu = 2,
  Share = 3,
  LeftStick = 4,
  RightStick = 5,
  A = 6,
  B = 7,
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
  int16_t dpadX{0};
  int16_t dpadY{0};
  uint8_t triggerLeft{0U};
  uint8_t triggerRight{0U};
  bool linkEncrypted{false};
  bool inputSubscribed{false};
  bool controllerPaired{false};
  char controllerName[32]{0};
};

class LeaderXboxControllerService {
public:
  // Initialize NimBLE scan/connect workflow and start the Xbox background task.
  void begin();
  // Refresh runtime status using current link/subscription/report freshness.
  void tick();
  // Return true when encrypted link and input subscription are both active.
  bool isControllerPaired() const;
  // Copy the latest runtime/input snapshot for telemetry serialization.
  void snapshot(XboxRuntimeSnapshot &out) const;
  // Override runtime state from callback or connection workflow code.
  void setRuntimeState(XboxRuntimeState state);
  // Mark input-report reception time and increment report counter.
  void markInputReport();
  // Cache discovered device name for diagnostics/UI.
  void onDeviceFound(const char *name);
  // Decode one HID input report and update button/axis runtime state.
  void updateInputState(const uint8_t *data, size_t length);
  // Return true when controller input path is fully operational for teleop.
  bool isReadyForTeleop() const;
  // Configure which logical button triggers operation-profile cycling.
  void setModeCycleButton(XboxLogicalButton button);
  // Consume one pending mode-cycle edge event.
  bool consumeModeCycleRequest();
  // Consume one pending edge event for a logical button (currently A/B).
  bool consumeButtonPress(XboxLogicalButton button);
  void discardPendingButtonPress(XboxLogicalButton button);

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
  std::atomic<int16_t> dpadX_{0};
  std::atomic<int16_t> dpadY_{0};
  std::atomic<uint8_t> triggerLeft_{0U};
  std::atomic<uint8_t> triggerRight_{0U};
  std::atomic<uint8_t> modeCycleButton_{static_cast<uint8_t>(XboxLogicalButton::View)};
  std::atomic<uint8_t> modeCyclePendingCount_{0U};
  std::atomic<bool> modeCyclePressedLast_{false};
  std::atomic<uint8_t> buttonAPendingCount_{0U};
  std::atomic<uint8_t> buttonBPendingCount_{0U};
  std::atomic<bool> buttonAPressedLast_{false};
  std::atomic<bool> buttonBPressedLast_{false};
  char controllerName_[32]{0};
  bool started_{false};
};

} // namespace soarm
