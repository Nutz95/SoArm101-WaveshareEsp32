#pragma once

#include <cstdint>

namespace soarm {

enum class OperationMode : uint8_t {
  Idle = 0,
  CalibrationLeader,
  CalibrationFollower,
  Teleoperation,
  Passthrough
};

} // namespace soarm
