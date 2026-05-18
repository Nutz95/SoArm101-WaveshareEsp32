#include "nvs_calibration_store.h"

namespace soarm {

bool NvsCalibrationStore::begin() {
  return preferences_.begin("soarm-cal", false);
}

bool NvsCalibrationStore::load(ArmRole role, CalibrationProfile &outProfile) {
  const char *key = roleKey(role);
  const size_t expectedSize = sizeof(CalibrationProfile);
  const size_t actualSize = preferences_.getBytesLength(key);

  if (actualSize != expectedSize) {
    return false;
  }

  const size_t loadedSize = preferences_.getBytes(key, &outProfile, expectedSize);
  return loadedSize == expectedSize;
}

bool NvsCalibrationStore::save(ArmRole role, const CalibrationProfile &profile) {
  const char *key = roleKey(role);
  const size_t expectedSize = sizeof(CalibrationProfile);
  const size_t savedSize = preferences_.putBytes(key, &profile, expectedSize);
  return savedSize == expectedSize;
}

CalibrationProfile NvsCalibrationStore::buildDefaultProfile() const {
  CalibrationProfile profile{};

  for (uint8_t servoIndex = 0; servoIndex < CalibrationProfile::kServoCount; servoIndex++) {
    profile.minPosition[servoIndex] = 0;
    profile.maxPosition[servoIndex] = 4095;
  }

  return profile;
}

const char *NvsCalibrationStore::roleKey(ArmRole role) const {
  if (role == ArmRole::Leader) {
    return "leader";
  }

  return "follower";
}

} // namespace soarm
