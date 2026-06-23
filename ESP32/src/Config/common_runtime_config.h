#pragma once

#include <cstdint>

#ifndef EXPECTED_LEADER_SERVO_COUNT
#define EXPECTED_LEADER_SERVO_COUNT 6U
#endif

#ifndef EXPECTED_FOLLOWER_SERVO_COUNT
#define EXPECTED_FOLLOWER_SERVO_COUNT 6U
#endif

namespace soarm {
namespace config {
namespace common {

constexpr uint32_t kStatusLedBlinkPeriodMs = 450U;
constexpr uint8_t kServoBusIoTimeoutMs = 20U;
constexpr uint32_t kServoBusLockTimeoutMs = 20U;
constexpr uint8_t kTeleopBatchMaxServos = 6U;
constexpr int16_t kTeleopPositionClampMin = -32767;
constexpr int16_t kTeleopPositionClampMax = 32767;
constexpr uint32_t kServoTemperaturePollIntervalMs = 3000U;
constexpr int16_t kServoTemperatureAlarmThresholdC = 70;
constexpr int16_t kServoTemperatureAlarmClearThresholdC = 65;
constexpr uint8_t kServoSetIdVerifyAttempts = 6U;
constexpr uint32_t kServoSetIdVerifyRetryDelayMs = 15U;
constexpr uint8_t kExpectedLeaderServoCount = static_cast<uint8_t>(EXPECTED_LEADER_SERVO_COUNT);
constexpr uint8_t kExpectedFollowerServoCount = static_cast<uint8_t>(EXPECTED_FOLLOWER_SERVO_COUNT);
// Shared leader/follower teleop control cadence (~83 Hz).
constexpr uint32_t kTeleopControlPeriodMs = 12U;
// ESP-NOW turbo: same cadence as classic; mirror task does its own bus read (telemetry stays slow).
constexpr uint32_t kTeleopTurboControlPeriodMs = kTeleopControlPeriodMs;

} // namespace common
} // namespace config
} // namespace soarm
