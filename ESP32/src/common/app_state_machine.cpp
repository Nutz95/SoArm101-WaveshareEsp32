#include "app_state_machine.h"

namespace soarm {

ArmRuntimeState ArmStateMachine::computeState(const ArmStateInputs &inputs) const {
  if (inputs.joystickRequired && !inputs.joystickPaired) {
    return ArmRuntimeState::PairingOrUnpaired;
  }

  if (inputs.joystickRequired && inputs.joystickPaired && !inputs.calibrationDone) {
    return ArmRuntimeState::WaitingCalibration;
  }

  if (!inputs.joystickRequired && !inputs.calibrationDone) {
    return ArmRuntimeState::WaitingCalibration;
  }

  if (!inputs.espNowLinked) {
    if (inputs.joystickRequired) {
      return ArmRuntimeState::WaitingEspNow;
    }

    return ArmRuntimeState::WaitingEspNow;
  }

  return ArmRuntimeState::Ready;
}

} // namespace soarm
