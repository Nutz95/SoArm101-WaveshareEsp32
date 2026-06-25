#include "oled_menu_screen_base.h"

#include "oled_menu_navigation_result.h"

namespace soarm {

void OledMenuScreenBase::setNavigationSink(IOledMenuNavigationSink *sink) {
  navigationSink_ = sink;
}

void OledMenuScreenBase::setProfileActionsSink(IOledMenuProfileActions *sink) {
  profileActionsSink_ = sink;
}

void OledMenuScreenBase::onEnter() {
}

void OledMenuScreenBase::onExit() {
}

bool OledMenuScreenBase::onInput(OledMenuInputEvent event) {
  (void)event;
  return false;
}

void OledMenuScreenBase::render(const OledMenuContext &context, OledMenuRenderOutput &out) const {
  (void)context;
  clearOutput(out);
}

void OledMenuScreenBase::clearOutput(OledMenuRenderOutput &out) {
  for (uint8_t i = 0U; i < kOledMenuVisibleLines; ++i) {
    out.lines[i][0] = '\0';
  }
}

bool OledMenuScreenBase::applyNavigationResult(const OledMenuNavigationResult &result) {
  if (result.action == OledMenuNavigationResult::Action::ActivateProfile) {
    if (profileActionsSink_ == nullptr) {
      return false;
    }
    return profileActionsSink_->activateMenuProfile(result.profileSelection);
  }

  if (navigationSink_ == nullptr) {
    return false;
  }

  if (result.action == OledMenuNavigationResult::Action::Push) {
    return navigationSink_->pushScreen(result.screen);
  }
  if (result.action == OledMenuNavigationResult::Action::Pop) {
    return navigationSink_->popScreen();
  }
  return false;
}

} // namespace soarm
