#include "oled_menu_root_screen.h"

#include "oled_menu_navigation_result.h"
#include "oled_menu_screen_id.h"

namespace soarm {

const char *const *OledMenuRootScreen::labels() const {
  return kOledMenuRootLabels;
}

uint8_t OledMenuRootScreen::labelCount() const {
  return kOledMenuRootItemCount;
}

bool OledMenuRootScreen::acceptsModeDown() const {
  return true;
}

OledMenuNavigationResult OledMenuRootScreen::onItemActivated(uint8_t itemIndex) const {
  switch (itemIndex) {
  case kOledMenuRootInfoIndex:
    return OledMenuNavigationResult::push(OledMenuScreenId::InfoDetail);
  case kOledMenuRootPairingIndex:
    return OledMenuNavigationResult::push(OledMenuScreenId::PairingList);
  default:
    return OledMenuNavigationResult::none();
  }
}

} // namespace soarm
