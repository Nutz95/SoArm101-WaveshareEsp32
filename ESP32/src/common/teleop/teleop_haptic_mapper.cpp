#include "teleop_haptic_mapper.h"

namespace soarm {
namespace teleop_haptic {

uint16_t mapWireLoadToTorqueLimit(uint8_t wireLoad, bool isGripper) {
  const uint16_t minLimit = isGripper ? kGripperTorqueLimitMin : kTorqueLimitMin;
  if (wireLoad <= kWireLoadDeadband) {
    return minLimit;
  }

  uint32_t scaled = wireLoad;
  if (isGripper) {
    scaled = (scaled * kGripperGainNumerator) / kGripperGainDenominator;
    if (scaled > 127U) {
      scaled = 127U;
    }
  }

  const uint16_t maxLimit = isGripper ? kGripperTorqueLimitMax : kTorqueLimitMax;
  const uint32_t span = static_cast<uint32_t>(maxLimit - minLimit);
  const uint32_t torque = minLimit + (scaled * span) / 127U;
  return static_cast<uint16_t>(torque > maxLimit ? maxLimit : torque);
}

} // namespace teleop_haptic
} // namespace soarm
