#pragma once

#include <cstdint>

namespace soarm {

// Leader Xbox / dashboard operation profiles (stored in controllerOperationProfile_).
enum class ControllerOperationProfile : uint8_t {
  CalibrationLeader = 0,
  CalibrationFollower = 1,
  TeleopEspNow = 2,
  TeleopEspNowTurbo = 3,
  TeleopWifi = 4,
  Passthrough = 5,
  OtaReady = 6,
  TeleopEspNowFeedback = 7,
};

constexpr uint8_t kControllerOperationProfileCount = 8U;

inline bool isEspNowTeleopProfile(ControllerOperationProfile profile) {
  return profile == ControllerOperationProfile::TeleopEspNow ||
         profile == ControllerOperationProfile::TeleopEspNowTurbo ||
         profile == ControllerOperationProfile::TeleopEspNowFeedback;
}

inline bool isEspNowTeleopTurboProfile(ControllerOperationProfile profile) {
  return profile == ControllerOperationProfile::TeleopEspNowTurbo;
}

inline bool isEspNowTeleopFeedbackProfile(ControllerOperationProfile profile) {
  return profile == ControllerOperationProfile::TeleopEspNowFeedback;
}

inline bool usesEspNowTurboDownlinkProfile(ControllerOperationProfile profile) {
  return profile == ControllerOperationProfile::TeleopEspNowTurbo ||
         profile == ControllerOperationProfile::TeleopEspNowFeedback;
}

inline ControllerOperationProfile migrateLegacyControllerProfile(uint8_t raw) {
  if (raw <= 2U) {
    return static_cast<ControllerOperationProfile>(raw);
  }
  if (raw == 3U) {
    return ControllerOperationProfile::TeleopWifi;
  }
  if (raw == 4U) {
    return ControllerOperationProfile::Passthrough;
  }
  if (raw == 5U || raw == 6U) {
    return ControllerOperationProfile::OtaReady;
  }
  return ControllerOperationProfile::TeleopEspNow;
}

inline ControllerOperationProfile sanitizeControllerOperationProfile(uint8_t raw) {
  if (raw >= kControllerOperationProfileCount) {
    return migrateLegacyControllerProfile(raw);
  }
  return static_cast<ControllerOperationProfile>(raw);
}

inline uint8_t toProfileRaw(ControllerOperationProfile profile) {
  return static_cast<uint8_t>(profile);
}

} // namespace soarm
