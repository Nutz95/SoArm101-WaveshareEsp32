#pragma once

#include "ioled_menu_pairing_actions.h"
#include "oled_menu_screen_base.h"

namespace soarm {

// A confirms resetPairing(); B pops back to the pairing list.
class OledMenuPairingResetConfirmScreen : public OledMenuScreenBase {
public:
  void setPairingActionsSink(IOledMenuPairingActions *sink);

  bool onInput(OledMenuInputEvent event) override;
  void render(const OledMenuContext &context, OledMenuRenderOutput &out) const override;

private:
  IOledMenuPairingActions *pairingActionsSink_{nullptr};
};

} // namespace soarm
