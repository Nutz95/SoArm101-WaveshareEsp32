#include "oled_menu_calibration_screen.h"

#include "oled_menu_item_activation.h"

namespace soarm {

const char *const *OledMenuCalibrationScreen::labels() const {
  return kOledMenuCalibrationLabels;
}

uint8_t OledMenuCalibrationScreen::labelCount() const {
  return kOledMenuCalibrationItemCount;
}

OledMenuNavigationResult OledMenuCalibrationScreen::onItemActivated(uint8_t itemIndex) const {
  return lookupMenuItemAction(kOledMenuCalibrationItemActions, kOledMenuCalibrationItemCount, itemIndex);
}

} // namespace soarm
