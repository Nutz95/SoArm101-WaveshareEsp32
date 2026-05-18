#pragma once

#include "types/arm_runtime_state.h"
#include "types/arm_state_inputs.h"

namespace soarm {

class ArmStateMachine {
public:
  ArmRuntimeState computeState(const ArmStateInputs &inputs) const;
};

} // namespace soarm
