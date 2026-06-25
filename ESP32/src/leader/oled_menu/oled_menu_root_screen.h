#pragma once

#include "oled_menu_list_screen_base.h"
#include "oled_menu_root_items.h"

namespace soarm {

// Boot root menu: Info, Teleop, Passthrough, Calibration, Pairing, OTA.
class OledMenuRootScreen : public OledMenuListScreenBase {
protected:
  const char *const *labels() const override;
  uint8_t labelCount() const override;
  // View (Mode) at root moves the highlight down instead of changing profile.
  bool acceptsModeDown() const override;
  OledMenuNavigationResult onItemActivated(uint8_t itemIndex) const override;
};

} // namespace soarm
