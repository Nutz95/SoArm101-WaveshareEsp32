#pragma once

#include <cstdint>

#ifndef EXPECTED_LEADER_SERVO_COUNT
#define EXPECTED_LEADER_SERVO_COUNT 1U
#endif

#ifndef EXPECTED_FOLLOWER_SERVO_COUNT
#define EXPECTED_FOLLOWER_SERVO_COUNT 1U
#endif

namespace soarm {

constexpr uint8_t kExpectedLeaderServoCount = static_cast<uint8_t>(EXPECTED_LEADER_SERVO_COUNT);
constexpr uint8_t kExpectedFollowerServoCount = static_cast<uint8_t>(EXPECTED_FOLLOWER_SERVO_COUNT);

} // namespace soarm
