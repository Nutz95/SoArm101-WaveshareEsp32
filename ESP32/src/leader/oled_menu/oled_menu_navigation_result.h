#pragma once

#include "oled_menu_profile_selection.h"
#include "oled_menu_screen_id.h"

namespace soarm {

// Result of activating a list row; consumed by screen base / navigator.
struct OledMenuNavigationResult {
  enum class Action : uint8_t {
    None = 0,
    Push,
    Pop,
    ActivateProfile,
  };

  Action action{Action::None};
  OledMenuScreenId screen{OledMenuScreenId::Root};
  OledMenuProfileSelection profileSelection{OledMenuProfileSelection::TeleopEspNow};

  static OledMenuNavigationResult none();
  static OledMenuNavigationResult push(OledMenuScreenId screen);
  static OledMenuNavigationResult pop();
  static OledMenuNavigationResult activateProfile(OledMenuProfileSelection selection);
};

} // namespace soarm
