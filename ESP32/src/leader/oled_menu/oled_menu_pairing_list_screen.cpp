#include "oled_menu_pairing_list_screen.h"

#include "oled_menu_navigation_result.h"
#include "oled_menu_screen_id.h"

namespace soarm {

const char *const *OledMenuPairingListScreen::labels() const {
  return kOledMenuPairingLabels;
}

uint8_t OledMenuPairingListScreen::labelCount() const {
  return kOledMenuPairingItemCount;
}

OledMenuNavigationResult OledMenuPairingListScreen::onItemActivated(uint8_t itemIndex) const {
  if (itemIndex == kOledMenuPairingStatusIndex) {
    return OledMenuNavigationResult::push(OledMenuScreenId::PairingStatusDetail);
  }
  if (itemIndex == kOledMenuPairingBackIndex) {
    return OledMenuNavigationResult::pop();
  }
  return OledMenuNavigationResult::none();
}

} // namespace soarm
