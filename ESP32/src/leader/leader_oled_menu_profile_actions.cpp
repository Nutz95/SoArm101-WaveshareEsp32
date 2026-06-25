#include "leader_oled_menu_profile_actions.h"

#include "leader_app.h"

namespace soarm {

LeaderOledMenuProfileActions::LeaderOledMenuProfileActions(LeaderApp &app) : app_(app) {}

bool LeaderOledMenuProfileActions::activateMenuProfile(OledMenuProfileSelection selection) {
  return app_.activateProfileFromMenu(selection);
}

} // namespace soarm
