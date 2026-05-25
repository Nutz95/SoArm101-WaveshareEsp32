#pragma once

#include <cstdint>

#ifndef EXPECTED_LEADER_SERVO_COUNT
#define EXPECTED_LEADER_SERVO_COUNT 1U
#endif

#ifndef EXPECTED_FOLLOWER_SERVO_COUNT
#define EXPECTED_FOLLOWER_SERVO_COUNT 1U
#endif

namespace soarm {
namespace config {
namespace common {

constexpr uint32_t kStatusLedBlinkPeriodMs = 450U;
constexpr uint8_t kServoBusIoTimeoutMs = 20U;
constexpr uint32_t kServoBusLockTimeoutMs = 20U;
constexpr uint8_t kServoSetIdVerifyAttempts = 6U;
constexpr uint32_t kServoSetIdVerifyRetryDelayMs = 15U;
constexpr uint8_t kExpectedLeaderServoCount = static_cast<uint8_t>(EXPECTED_LEADER_SERVO_COUNT);
constexpr uint8_t kExpectedFollowerServoCount = static_cast<uint8_t>(EXPECTED_FOLLOWER_SERVO_COUNT);

} // namespace common
} // namespace config
} // namespace soarm
