#pragma once

#include <cstdint>

namespace soarm {
namespace teleop_haptic {

/// STS torque limit scale (0..1000). Low = easy to backdrive; high = strong hold.
constexpr uint16_t kTorqueLimitMin = 80U;
constexpr uint16_t kTorqueLimitMax = 500U;
constexpr uint16_t kGripperTorqueLimitMin = 320U;
constexpr uint16_t kGripperTorqueLimitMax = 1000U;
constexpr uint8_t kWireLoadDeadband = 1U;
constexpr uint8_t kGripperGainNumerator = 5U;
constexpr uint8_t kGripperGainDenominator = 2U;

/// Maps follower wire load (0..127) to an STS torque-limit setpoint for haptic hold.
uint16_t mapWireLoadToTorqueLimit(uint8_t wireLoad, bool isGripper);

} // namespace teleop_haptic
} // namespace soarm
