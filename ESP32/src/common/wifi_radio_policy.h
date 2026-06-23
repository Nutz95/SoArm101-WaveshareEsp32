#pragma once

#include "controller/controller_operation_profile.h"

namespace soarm {

// True when the board should stay associated to the home Wi-Fi AP (dashboard, OTA, Wi-Fi teleop).
inline bool controllerProfileNeedsWifiSta(ControllerOperationProfile profile) {
  switch (profile) {
  case ControllerOperationProfile::TeleopEspNow:
  case ControllerOperationProfile::TeleopEspNowTurbo:
    return false;
  default:
    return true;
  }
}

} // namespace soarm
