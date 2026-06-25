#include "oled_menu_navigation_result.h"

namespace soarm {

OledMenuNavigationResult OledMenuNavigationResult::none() {
  return OledMenuNavigationResult{};
}

OledMenuNavigationResult OledMenuNavigationResult::push(OledMenuScreenId screen) {
  OledMenuNavigationResult result{};
  result.action = Action::Push;
  result.screen = screen;
  return result;
}

OledMenuNavigationResult OledMenuNavigationResult::pop() {
  OledMenuNavigationResult result{};
  result.action = Action::Pop;
  return result;
}

OledMenuNavigationResult OledMenuNavigationResult::activateProfile(OledMenuProfileSelection selection) {
  OledMenuNavigationResult result{};
  result.action = Action::ActivateProfile;
  result.profileSelection = selection;
  return result;
}

} // namespace soarm
