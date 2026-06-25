#pragma once

#include "oled_menu_profile_selection.h"

namespace soarm {

// Callback from menu screens to apply a teleop/passthrough profile on the leader.
class IOledMenuProfileActions {
public:
  virtual ~IOledMenuProfileActions() = default;

  virtual bool activateMenuProfile(OledMenuProfileSelection selection) = 0;
};

} // namespace soarm
