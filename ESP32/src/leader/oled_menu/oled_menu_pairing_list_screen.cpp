#include "oled_menu_pairing_list_screen.h"

#include "oled_menu_item_activation.h"
#include "oled_menu_pairing_items.h"

namespace soarm {

const char *const *OledMenuPairingListScreen::labels() const {
  return kOledMenuPairingLabels;
}

uint8_t OledMenuPairingListScreen::labelCount() const {
  return kOledMenuPairingItemCount;
}

OledMenuNavigationResult OledMenuPairingListScreen::onItemActivated(uint8_t itemIndex) const {
  return lookupMenuItemAction(kOledMenuPairingItemActions, kOledMenuPairingItemCount, itemIndex);
}

} // namespace soarm
