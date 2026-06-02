#pragma once

#include <cstdint>

namespace soarm {
namespace config {
namespace leader {

constexpr uint32_t kTickDelayMs = 2U;
// Stagger heavy init so Wi-Fi + ESP-NOW stabilize before NimBLE (avoids boot loops).
constexpr uint32_t kDeferredServoBusMs = 800U;
constexpr uint32_t kDeferredNetworkMs = 1200U;
constexpr uint32_t kDeferredBackgroundTasksMs = 1500U;
constexpr uint32_t kDeferredBleMs = 10000U;
constexpr uint32_t kJoystickPairReadyMs = 3000U;
constexpr uint32_t kCalibrationReadyMs = 6000U;
constexpr bool kCalibrationRequired = false;
constexpr uint16_t kTeleopServoMaxSpeedRaw = 7000U;
constexpr uint8_t kTeleopContinuousSpeedPct = 100U;
// ~60 Hz teleop loop (LeRobot-aligned target).
constexpr uint32_t kTeleopTargetPeriodMs = 17U;
constexpr int16_t kTeleopMirrorMinPositionDelta = 3;
constexpr int16_t kTeleopMirrorMinPositionDeltaWifi = 0;
constexpr uint32_t kServoTelemetryTaskActiveDelayMs = kTeleopTargetPeriodMs;
constexpr uint32_t kServoTelemetryTaskIdleDelayMs = 10U;
constexpr uint32_t kServoTelemetryTaskCalibrationDelayMs = 20U;
constexpr uint32_t kTeleopMirrorTaskActiveDelayMs = kTeleopTargetPeriodMs;
constexpr uint32_t kTeleopMirrorTaskIdleDelayMs = 1U;
constexpr uint32_t kTeleopMirrorTaskWifiActiveDelayMs = kTeleopTargetPeriodMs;
constexpr uint8_t kTeleopWifiMaxPendingBatches = 6U;
constexpr uint32_t kTeleopWifiSendErrorLogIntervalMs = 1000U;
constexpr uint32_t kTeleopWifiSendBackoffMs = 50U;
constexpr bool kTeleopWifiRequireAck = false;
constexpr uint32_t kTelemetryStreamPeriodMs = 10U;
constexpr uint32_t kTelemetrySnapshotTeleopPeriodMs = 10U;
constexpr uint32_t kTelemetrySnapshotIdlePeriodMs = 100U;

constexpr uint32_t kFollowerScanRetryIntervalMs = 1200U;
constexpr uint8_t kFollowerCommandMaxRetries = 6U;
constexpr uint32_t kFollowerRetryIntervalMs = 200U;
constexpr uint8_t kFollowerInitialSendBurstCount = 6U;
constexpr uint32_t kFollowerAckDeadlineSlackMs = 600U;
constexpr uint8_t kFollowerAckRttClampMs = 250U;

constexpr uint32_t kFollowerDebugAckTimeoutMs = 3500U;
constexpr uint32_t kFollowerMoveAckTimeoutMs = 3500U;
constexpr uint32_t kFollowerCalibrationCenterAckTimeoutMs = 180000U;
// Center capture is on button A only (no timed auto-center).
constexpr uint32_t kCalibrationConfirmArmDelayMs = 0U;
constexpr uint32_t kWifiDirectAckTimeoutMs = 25000U;
constexpr uint32_t kWifiDirectOfferResendMs = 5000U;
constexpr uint32_t kFollowerSetIdAckTimeoutMs = 2000U;
constexpr uint32_t kFollowerSetModeAckTimeoutMs = 1500U;
constexpr uint32_t kFollowerScanAckTimeoutMs = 3000U;

// Xbox BLE controller runtime.
constexpr uint32_t kXboxScanWindowMs = 4000U;
constexpr uint32_t kXboxScanRetryDelayMs = 1000U;
constexpr uint32_t kXboxConnectedTickDelayMs = 20U;
constexpr uint32_t kXboxDisconnectedTickDelayMs = 200U;
constexpr uint32_t kXboxInputReportStaleMs = 2000U;

constexpr uint32_t kResetPairingStatusHoldMs = 2500U;
constexpr uint32_t kScanStatusHoldMs = 2200U;
constexpr uint32_t kDebugStatusHoldMs = 2000U;
constexpr uint32_t kMoveStatusHoldMs = 1800U;
constexpr uint32_t kSetIdStatusHoldMs = 2200U;
constexpr uint32_t kSetModeStatusHoldMs = 1800U;

// Pairing management.
constexpr uint32_t kPairingTimeoutMs = 45000U;        // no presence from follower → expire pairing
constexpr uint32_t kResetBroadcastIntervalMs = 100U;  // delay between scheduled PairReset broadcasts

} // namespace leader
} // namespace config
} // namespace soarm
