#pragma once

#include "oled_menu/ioled_menu_profile_actions.h"

namespace soarm {

class LeaderApp;

// Forwards OLED menu profile leaves to LeaderApp::activateProfileFromMenu.
class LeaderOledMenuProfileActions : public IOledMenuProfileActions {
public:
  explicit LeaderOledMenuProfileActions(LeaderApp &app);

  bool activateMenuProfile(OledMenuProfileSelection selection) override;

private:
  LeaderApp &app_;
};

} // namespace soarm
