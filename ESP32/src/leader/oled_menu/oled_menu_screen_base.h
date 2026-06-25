#pragma once

#include "ioled_menu_profile_actions.h"
#include "ioled_menu_screen.h"
#include "oled_menu_navigation_result.h"
#include "ioled_menu_navigation_sink.h"
#include "oled_menu_render_output.h"

namespace soarm {

// Default IOledMenuScreen: empty render and no input handling.
// Subclasses override only the methods they need.
class OledMenuScreenBase : public IOledMenuScreen {
public:
  // Required for list screens to request push/pop on the navigator stack.
  void setNavigationSink(IOledMenuNavigationSink *sink);
  // Required for teleop/passthrough leaves to apply ControllerOperationProfile.
  void setProfileActionsSink(IOledMenuProfileActions *sink);

  void onEnter() override;
  void onExit() override;
  bool onInput(OledMenuInputEvent event) override;
  void render(const OledMenuContext &context, OledMenuRenderOutput &out) const override;

  static void clearOutput(OledMenuRenderOutput &out);

protected:
  IOledMenuNavigationSink *navigationSink_{nullptr};
  IOledMenuProfileActions *profileActionsSink_{nullptr};
  bool applyNavigationResult(const OledMenuNavigationResult &result);
};

} // namespace soarm
