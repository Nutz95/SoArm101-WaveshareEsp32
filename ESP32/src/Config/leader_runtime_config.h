#pragma once

#include <cstdint>

namespace soarm {
namespace config {
namespace leader {

constexpr uint32_t kTickDelayMs = 10U;
constexpr uint32_t kJoystickPairReadyMs = 3000U;
constexpr uint32_t kCalibrationReadyMs = 6000U;
constexpr bool kCalibrationRequired = false;
constexpr uint16_t kTeleopServoMaxSpeedRaw = 7000U;
constexpr uint8_t kTeleopContinuousSpeedPct = 100U;
constexpr uint32_t kServoTelemetryTaskActiveDelayMs = 5U;
constexpr uint32_t kServoTelemetryTaskIdleDelayMs = 10U;
constexpr uint32_t kTeleopMirrorTaskActiveDelayMs = 5U;
constexpr uint32_t kTeleopMirrorTaskIdleDelayMs = 10U;

constexpr uint32_t kFollowerScanRetryIntervalMs = 1200U;
constexpr uint8_t kFollowerCommandMaxRetries = 6U;
constexpr uint32_t kFollowerRetryIntervalMs = 450U;
constexpr uint8_t kFollowerInitialSendBurstCount = 6U;
constexpr uint32_t kFollowerAckDeadlineSlackMs = 600U;
constexpr uint8_t kFollowerAckRttClampMs = 250U;

constexpr uint32_t kFollowerDebugAckTimeoutMs = 3500U;
constexpr uint32_t kFollowerMoveAckTimeoutMs = 1500U;
constexpr uint32_t kFollowerSetIdAckTimeoutMs = 2000U;
constexpr uint32_t kFollowerSetModeAckTimeoutMs = 1500U;
constexpr uint32_t kFollowerScanAckTimeoutMs = 3000U;

constexpr uint32_t kResetPairingStatusHoldMs = 2500U;
constexpr uint32_t kScanStatusHoldMs = 2200U;
constexpr uint32_t kDebugStatusHoldMs = 2000U;
constexpr uint32_t kMoveStatusHoldMs = 1800U;
constexpr uint32_t kSetIdStatusHoldMs = 2200U;
constexpr uint32_t kSetModeStatusHoldMs = 1800U;

// Pairing management.
constexpr uint32_t kPairingTimeoutMs = 15000U;        // no contact from follower → expire pairing
constexpr uint32_t kResetBroadcastIntervalMs = 100U;  // delay between scheduled PairReset broadcasts

} // namespace leader
} // namespace config
} // namespace soarm
