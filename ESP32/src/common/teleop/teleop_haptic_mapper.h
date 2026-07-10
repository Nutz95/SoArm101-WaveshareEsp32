#pragma once

#include <cstdint>

namespace soarm {
namespace teleop_haptic {

/// STS torque limit scale (0..1000). Low = easy to backdrive; high = strong hold.
constexpr uint16_t kTorqueLimitMin = 80U;
constexpr uint16_t kTorqueLimitMax = 500U;
constexpr uint16_t kGripperTorqueLimitMax = 750U;
constexpr uint8_t kWireLoadDeadband = 4U;
constexpr uint8_t kGripperGainNumerator = 3U;
constexpr uint8_t kGripperGainDenominator = 2U;

uint16_t mapWireLoadToTorqueLimit(uint8_t wireLoad, bool isGripper);

} // namespace teleop_haptic
} // namespace soarm
