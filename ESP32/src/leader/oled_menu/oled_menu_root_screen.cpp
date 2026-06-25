#include "oled_menu_root_screen.h"

#include "oled_menu_item_activation.h"
#include "oled_menu_root_items.h"

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
  return lookupMenuItemAction(kOledMenuRootItemActions, kOledMenuRootItemCount, itemIndex);
}

} // namespace soarm
