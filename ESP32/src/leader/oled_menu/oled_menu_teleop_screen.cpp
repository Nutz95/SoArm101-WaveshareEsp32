#include "oled_menu_teleop_screen.h"

#include "oled_menu_item_activation.h"
#include "oled_menu_teleop_items.h"

namespace soarm {

const char *const *OledMenuTeleopScreen::labels() const {
  return kOledMenuTeleopLabels;
}

uint8_t OledMenuTeleopScreen::labelCount() const {
  return kOledMenuTeleopItemCount;
}

OledMenuNavigationResult OledMenuTeleopScreen::onItemActivated(uint8_t itemIndex) const {
  return lookupMenuItemAction(kOledMenuTeleopItemActions, kOledMenuTeleopItemCount, itemIndex);
}

} // namespace soarm
