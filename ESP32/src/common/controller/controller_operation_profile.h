#pragma once

#include <cstdint>

namespace soarm {

// Leader Xbox / dashboard operation profiles (stored in controllerOperationProfile_).
enum class ControllerOperationProfile : uint8_t {
  CalibrationLeader = 0,
  CalibrationFollower = 1,
  TeleopEspNow = 2,
  TeleopWifi = 3,
  Passthrough = 4,
  TeleopPcSerial = 5,
};

constexpr uint8_t kControllerOperationProfileCount = 6U;

inline ControllerOperationProfile sanitizeControllerOperationProfile(uint8_t raw) {
  if (raw >= kControllerOperationProfileCount) {
    return ControllerOperationProfile::TeleopEspNow;
  }
  return static_cast<ControllerOperationProfile>(raw);
}

inline uint8_t toProfileRaw(ControllerOperationProfile profile) {
  return static_cast<uint8_t>(profile);
}

} // namespace soarm
