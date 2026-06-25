#pragma once

#include "common/controller/controller_operation_profile.h"
#include "oled_menu_profile_selection.h"

#include <cstdint>

namespace soarm {

constexpr uint8_t kOledMenuProfileSelectionCount = 5U;

constexpr ControllerOperationProfile kControllerProfileByMenuSelection[] = {
    ControllerOperationProfile::TeleopEspNow,
    ControllerOperationProfile::TeleopEspNowTurbo,
    ControllerOperationProfile::TeleopWifi,
    ControllerOperationProfile::Passthrough,
    ControllerOperationProfile::OtaReady,
};

inline ControllerOperationProfile controllerProfileForMenuSelection(OledMenuProfileSelection selection) {
  const uint8_t index = static_cast<uint8_t>(selection);
  if (index >= kOledMenuProfileSelectionCount) {
    return ControllerOperationProfile::TeleopEspNow;
  }
  return kControllerProfileByMenuSelection[index];
}

} // namespace soarm
