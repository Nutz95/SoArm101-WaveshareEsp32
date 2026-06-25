#pragma once

#include "oled_menu_list_screen_base.h"
#include "oled_menu_teleop_items.h"

namespace soarm {

// Teleop transport submenu: ESP-NOW, Turbo, Wi-Fi, IK stub, Back.
class OledMenuTeleopScreen : public OledMenuListScreenBase {
protected:
  const char *const *labels() const override;
  uint8_t labelCount() const override;
  OledMenuNavigationResult onItemActivated(uint8_t itemIndex) const override;
};

} // namespace soarm
