#pragma once

#include "oled_menu/ioled_menu_pairing_actions.h"

namespace soarm {

class LeaderApp;

// Forwards OLED pairing reset confirm to LeaderApp::resetPairingFromMenu.
class LeaderOledMenuPairingActions : public IOledMenuPairingActions {
public:
  explicit LeaderOledMenuPairingActions(LeaderApp &app);

  bool resetPairingFromMenu() override;

private:
  LeaderApp &app_;
};

} // namespace soarm
