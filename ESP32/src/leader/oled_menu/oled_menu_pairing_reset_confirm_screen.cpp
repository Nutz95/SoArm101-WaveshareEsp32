#include "oled_menu_pairing_reset_confirm_screen.h"

#include "oled_menu_input.h"
#include "oled_menu_text_utils.h"

namespace soarm {

void OledMenuPairingResetConfirmScreen::setPairingActionsSink(IOledMenuPairingActions *sink) {
  pairingActionsSink_ = sink;
}

bool OledMenuPairingResetConfirmScreen::onInput(OledMenuInputEvent event) {
  if (navigationSink_ == nullptr) {
    return false;
  }

  if (event == OledMenuInputEvent::Back) {
    return navigationSink_->popScreen();
  }

  if (event == OledMenuInputEvent::Select) {
    if (pairingActionsSink_ != nullptr) {
      (void)pairingActionsSink_->resetPairingFromMenu();
    }
    return navigationSink_->popScreen();
  }

  return false;
}

void OledMenuPairingResetConfirmScreen::render(const OledMenuContext &context,
                                               OledMenuRenderOutput &out) const {
  (void)context;
  clearOutput(out);
  oledMenuCopyLine(out.lines[0], kOledMenuLineChars, "Reset pairing?");
  oledMenuCopyLine(out.lines[1], kOledMenuLineChars, "Clears ESP-NOW");
  oledMenuCopyLine(out.lines[2], kOledMenuLineChars, "A: confirm");
  oledMenuCopyLine(out.lines[3], kOledMenuLineChars, "B: back");
}

} // namespace soarm
