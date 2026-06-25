#pragma once

#include "oled_menu_screen_id.h"

namespace soarm {

// Result of activating a list row; consumed by OledMenuNavigator to push or pop.
struct OledMenuNavigationResult {
  enum class Action : uint8_t {
    None = 0,
    Push,
    Pop,
  };

  Action action{Action::None};
  OledMenuScreenId screen{OledMenuScreenId::Root};

  static OledMenuNavigationResult none();
  static OledMenuNavigationResult push(OledMenuScreenId screen);
  static OledMenuNavigationResult pop();
};

} // namespace soarm
