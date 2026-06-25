#include "leader_oled_menu_pairing_actions.h"

#include "leader_app.h"

namespace soarm {

LeaderOledMenuPairingActions::LeaderOledMenuPairingActions(LeaderApp &app) : app_(app) {}

bool LeaderOledMenuPairingActions::resetPairingFromMenu() {
  return app_.resetPairingFromMenu();
}

} // namespace soarm
