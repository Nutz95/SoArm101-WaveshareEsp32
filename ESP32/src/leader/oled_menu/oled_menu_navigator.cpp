#include "oled_menu_navigator.h"

#include "oled_menu_screen_base.h"

namespace soarm {

OledMenuNavigator::OledMenuNavigator() {
  wireScreenSinks();
}

void OledMenuNavigator::reset() {
  stackDepth_ = 1U;
  stack_[0] = OledMenuScreenId::Root;
  rootScreen_.onEnter();
}

bool OledMenuNavigator::onInput(OledMenuInputEvent event) {
  if (event == OledMenuInputEvent::Back) {
    return popScreen();
  }

  IOledMenuScreen *screen = activeScreen();
  if (screen == nullptr) {
    return false;
  }
  return screen->onInput(event);
}

void OledMenuNavigator::render(const OledMenuContext &context, OledMenuRenderOutput &out) const {
  const IOledMenuScreen *screen = activeScreen();
  if (screen == nullptr) {
    OledMenuScreenBase::clearOutput(out);
    return;
  }
  screen->render(context, out);
}

bool OledMenuNavigator::isAtRoot() const {
  return isNavigatorAtRoot() && currentScreen() == OledMenuScreenId::Root;
}

OledMenuScreenId OledMenuNavigator::currentScreen() const {
  if (stackDepth_ == 0U) {
    return OledMenuScreenId::Root;
  }
  return stack_[static_cast<uint8_t>(stackDepth_ - 1U)];
}

bool OledMenuNavigator::pushScreen(OledMenuScreenId screen) {
  if (stackDepth_ >= kMaxStackDepth) {
    return false;
  }

  IOledMenuScreen *leaving = activeScreen();
  if (leaving != nullptr) {
    leaving->onExit();
  }

  stack_[stackDepth_] = screen;
  stackDepth_ = static_cast<uint8_t>(stackDepth_ + 1U);

  IOledMenuScreen *entering = activeScreen();
  if (entering != nullptr) {
    entering->onEnter();
  }
  return true;
}

bool OledMenuNavigator::popScreen() {
  if (stackDepth_ <= 1U) {
    return false;
  }

  IOledMenuScreen *leaving = activeScreen();
  if (leaving != nullptr) {
    leaving->onExit();
  }

  stackDepth_ = static_cast<uint8_t>(stackDepth_ - 1U);
  return true;
}

bool OledMenuNavigator::isNavigatorAtRoot() const {
  return stackDepth_ <= 1U;
}

void OledMenuNavigator::wireScreenSinks() {
  rootScreen_.setNavigationSink(this);
  infoDetailScreen_.setNavigationSink(this);
  pairingListScreen_.setNavigationSink(this);
  pairingStatusDetailScreen_.setNavigationSink(this);
}

IOledMenuScreen *OledMenuNavigator::screenForId(OledMenuScreenId screenId) {
  switch (screenId) {
  case OledMenuScreenId::Root:
    return &rootScreen_;
  case OledMenuScreenId::InfoDetail:
    return &infoDetailScreen_;
  case OledMenuScreenId::PairingList:
    return &pairingListScreen_;
  case OledMenuScreenId::PairingStatusDetail:
    return &pairingStatusDetailScreen_;
  default:
    return nullptr;
  }
}

const IOledMenuScreen *OledMenuNavigator::screenForId(OledMenuScreenId screenId) const {
  return const_cast<OledMenuNavigator *>(this)->screenForId(screenId);
}

IOledMenuScreen *OledMenuNavigator::activeScreen() {
  return screenForId(currentScreen());
}

const IOledMenuScreen *OledMenuNavigator::activeScreen() const {
  return screenForId(currentScreen());
}

} // namespace soarm
