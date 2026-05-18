#pragma once

#include "types/arm_role.h"
#include "types/calibration_profile.h"
#include <Preferences.h>

namespace soarm {

class NvsCalibrationStore {
public:
  bool begin();
  bool load(ArmRole role, CalibrationProfile &outProfile);
  bool save(ArmRole role, const CalibrationProfile &profile);
  CalibrationProfile buildDefaultProfile() const;

private:
  const char *roleKey(ArmRole role) const;
  Preferences preferences_;
};

} // namespace soarm
