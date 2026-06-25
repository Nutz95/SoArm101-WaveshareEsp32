#pragma once

#include "oled_menu_navigation_result.h"

#include <cstdint>

namespace soarm {

inline OledMenuNavigationResult lookupMenuItemAction(
    const OledMenuNavigationResult *actions,
    uint8_t actionCount,
    uint8_t itemIndex) {
  if (actions == nullptr || itemIndex >= actionCount) {
    return OledMenuNavigationResult::none();
  }
  return actions[itemIndex];
}

} // namespace soarm
