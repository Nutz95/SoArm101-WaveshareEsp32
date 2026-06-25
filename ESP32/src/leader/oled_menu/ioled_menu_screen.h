#pragma once

#include "oled_menu_context.h"
#include "oled_menu_input.h"
#include "oled_menu_render_output.h"

namespace soarm {

// Contract for one navigable OLED screen (State pattern).
struct IOledMenuScreen {
  virtual ~IOledMenuScreen() = default;

  // Called when the screen becomes the top of the navigation stack.
  virtual void onEnter() = 0;
  // Called when the screen is popped or covered by a push.
  virtual void onExit() = 0;
  // Handle one menu input event; return true when the UI should refresh.
  virtual bool onInput(OledMenuInputEvent event) = 0;
  // Fill four text lines for the 128x32 display.
  virtual void render(const OledMenuContext &context, OledMenuRenderOutput &out) const = 0;
};

} // namespace soarm
