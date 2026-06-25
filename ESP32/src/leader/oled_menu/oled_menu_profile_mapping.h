#pragma once

#include "common/controller/controller_operation_profile.h"
#include "oled_menu_profile_selection.h"

#include <cstdint>

namespace soarm {

constexpr ControllerOperationProfile kControllerProfileByMenuSelection[kOledMenuProfileSelectionCount] = {
    [static_cast<uint8_t>(OledMenuProfileSelection::TeleopEspNow)] =
        ControllerOperationProfile::TeleopEspNow,
    [static_cast<uint8_t>(OledMenuProfileSelection::TeleopEspNowTurbo)] =
        ControllerOperationProfile::TeleopEspNowTurbo,
    [static_cast<uint8_t>(OledMenuProfileSelection::TeleopWifi)] =
        ControllerOperationProfile::TeleopWifi,
    [static_cast<uint8_t>(OledMenuProfileSelection::Passthrough)] =
        ControllerOperationProfile::Passthrough,
    [static_cast<uint8_t>(OledMenuProfileSelection::OtaReady)] = ControllerOperationProfile::OtaReady,
    [static_cast<uint8_t>(OledMenuProfileSelection::CalibrationLeader)] =
        ControllerOperationProfile::CalibrationLeader,
    [static_cast<uint8_t>(OledMenuProfileSelection::CalibrationFollower)] =
        ControllerOperationProfile::CalibrationFollower,
};

inline ControllerOperationProfile controllerProfileForMenuSelection(OledMenuProfileSelection selection) {
  const uint8_t index = static_cast<uint8_t>(selection);
  if (index >= kOledMenuProfileSelectionCount) {
    return ControllerOperationProfile::TeleopEspNow;
  }
  return kControllerProfileByMenuSelection[index];
}

} // namespace soarm
