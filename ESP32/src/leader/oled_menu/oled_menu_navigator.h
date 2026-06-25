#pragma once

#include "ioled_menu_screen.h"
#include "ioled_menu_navigation_sink.h"
#include "oled_menu_context.h"
#include "oled_menu_info_detail_screen.h"
#include "oled_menu_input.h"
#include "oled_menu_pairing_list_screen.h"
#include "oled_menu_pairing_status_detail_screen.h"
#include "oled_menu_render_output.h"
#include "oled_menu_ik_not_implemented_screen.h"
#include "oled_menu_teleop_screen.h"
#include "oled_menu_root_screen.h"
#include "oled_menu_screen_id.h"

#include <cstdint>

namespace soarm {

// Owns the screen stack and delegates render/input to the active IOledMenuScreen.
class OledMenuNavigator : public IOledMenuNavigationSink {
public:
  OledMenuNavigator();

  // Clear the stack and show the root menu.
  void reset();
  // Re-open browse mode at a submenu (Root + screen) without losing the parent context.
  void resumeAt(OledMenuScreenId screen);
  // Forward one Xbox menu event to the active screen; Back pops when depth > 1.
  bool onInput(OledMenuInputEvent event);
  // Draw the active screen into four 22-character lines.
  void render(const OledMenuContext &context, OledMenuRenderOutput &out) const;
  // True when only the root screen is on the stack.
  bool isAtRoot() const;
  // Identity of the screen currently on top of the stack.
  OledMenuScreenId currentScreen() const;

  // Wire profile activation (teleop/passthrough leaves) to LeaderApp.
  void setProfileActionsSink(IOledMenuProfileActions *sink);

  bool pushScreen(OledMenuScreenId screen) override;
  bool popScreen() override;
  bool isNavigatorAtRoot() const override;

private:
  static constexpr uint8_t kMaxStackDepth = 4U;

  void wireScreenSinks();
  IOledMenuScreen *screenForId(OledMenuScreenId screenId);
  const IOledMenuScreen *screenForId(OledMenuScreenId screenId) const;
  IOledMenuScreen *activeScreen();
  const IOledMenuScreen *activeScreen() const;

  OledMenuRootScreen rootScreen_;
  OledMenuTeleopScreen teleopScreen_;
  OledMenuIkNotImplementedScreen ikNotImplementedScreen_;
  OledMenuInfoDetailScreen infoDetailScreen_;
  OledMenuPairingListScreen pairingListScreen_;
  OledMenuPairingStatusDetailScreen pairingStatusDetailScreen_;

  uint8_t stackDepth_{0U};
  OledMenuScreenId stack_[kMaxStackDepth]{OledMenuScreenId::Root};
};

} // namespace soarm
