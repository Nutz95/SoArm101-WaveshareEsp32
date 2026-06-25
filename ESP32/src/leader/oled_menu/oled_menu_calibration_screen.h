#pragma once

#include "oled_menu_calibration_items.h"
#include "oled_menu_list_screen_base.h"

namespace soarm {

// Calibration role submenu: Leader, Follower, Back.
class OledMenuCalibrationScreen : public OledMenuListScreenBase {
protected:
  const char *const *labels() const override;
  uint8_t labelCount() const override;
  OledMenuNavigationResult onItemActivated(uint8_t itemIndex) const override;
};

} // namespace soarm
