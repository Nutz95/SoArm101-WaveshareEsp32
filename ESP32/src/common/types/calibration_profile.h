#pragma once

#include <cstdint>

#include "arm_servo_count.h"

namespace soarm {

struct CalibrationProfile {
  static constexpr uint8_t kServoCount = ARM_SERVO_COUNT;
  uint16_t minPosition[kServoCount];
  uint16_t maxPosition[kServoCount];
};

} // namespace soarm
