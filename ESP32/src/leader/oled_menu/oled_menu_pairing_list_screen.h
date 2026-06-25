#pragma once

#include "oled_menu_list_screen_base.h"
#include "oled_menu_root_items.h"

namespace soarm {

// Pairing submenu: Status and Back.
class OledMenuPairingListScreen : public OledMenuListScreenBase {
protected:
  const char *const *labels() const override;
  uint8_t labelCount() const override;
  OledMenuNavigationResult onItemActivated(uint8_t itemIndex) const override;
};

} // namespace soarm
