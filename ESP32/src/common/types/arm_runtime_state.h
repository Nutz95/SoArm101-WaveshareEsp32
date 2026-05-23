#pragma once

#include <cstdint>

namespace soarm {

enum class ArmRuntimeState : uint8_t {
  PairingOrUnpaired = 0,
  Paired,
  WaitingCalibration,
  WaitingEspNow,
  Ready,
  ServoFault
};

} // namespace soarm
