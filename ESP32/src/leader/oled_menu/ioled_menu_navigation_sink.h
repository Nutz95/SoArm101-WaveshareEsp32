#pragma once

#include "oled_menu_screen_id.h"

namespace soarm {

// Screens call this to push or pop without depending on OledMenuNavigator.
class IOledMenuNavigationSink {
public:
  virtual ~IOledMenuNavigationSink() = default;

  virtual bool pushScreen(OledMenuScreenId screen) = 0;
  virtual bool popScreen() = 0;
  virtual bool isNavigatorAtRoot() const = 0;
};

} // namespace soarm
