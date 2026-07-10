#include "teleop_haptic_mapper.h"

namespace soarm {
namespace teleop_haptic {

uint16_t mapWireLoadToTorqueLimit(uint8_t wireLoad, bool isGripper) {
  if (wireLoad <= kWireLoadDeadband) {
    return kTorqueLimitMin;
  }

  uint32_t scaled = wireLoad;
  if (isGripper) {
    scaled = (scaled * kGripperGainNumerator) / kGripperGainDenominator;
    if (scaled > 127U) {
      scaled = 127U;
    }
  }

  const uint16_t maxLimit = isGripper ? kGripperTorqueLimitMax : kTorqueLimitMax;
  const uint32_t span = static_cast<uint32_t>(maxLimit - kTorqueLimitMin);
  const uint32_t torque = kTorqueLimitMin + (scaled * span) / 127U;
  return static_cast<uint16_t>(torque > maxLimit ? maxLimit : torque);
}

} // namespace teleop_haptic
} // namespace soarm
