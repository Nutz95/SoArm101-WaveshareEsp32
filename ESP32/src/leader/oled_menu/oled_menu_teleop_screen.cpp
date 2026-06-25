#include "oled_menu_teleop_screen.h"

#include "oled_menu_navigation_result.h"
#include "oled_menu_profile_selection.h"
#include "oled_menu_screen_id.h"

namespace soarm {

const char *const *OledMenuTeleopScreen::labels() const {
  return kOledMenuTeleopLabels;
}

uint8_t OledMenuTeleopScreen::labelCount() const {
  return kOledMenuTeleopItemCount;
}

OledMenuNavigationResult OledMenuTeleopScreen::onItemActivated(uint8_t itemIndex) const {
  switch (itemIndex) {
  case kOledMenuTeleopEspNowIndex:
    return OledMenuNavigationResult::activateProfile(OledMenuProfileSelection::TeleopEspNow);
  case kOledMenuTeleopEspNowTurboIndex:
    return OledMenuNavigationResult::activateProfile(OledMenuProfileSelection::TeleopEspNowTurbo);
  case kOledMenuTeleopWifiIndex:
    return OledMenuNavigationResult::activateProfile(OledMenuProfileSelection::TeleopWifi);
  case kOledMenuTeleopIkIndex:
    return OledMenuNavigationResult::push(OledMenuScreenId::IkNotImplementedDetail);
  case kOledMenuTeleopBackIndex:
    return OledMenuNavigationResult::pop();
  default:
    return OledMenuNavigationResult::none();
  }
}

} // namespace soarm
