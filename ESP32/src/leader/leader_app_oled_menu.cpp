#include "leader_app.h"

#include "../common/controller/controller_operation_profile.h"
#include "leader_presence_service.h"
#include "oled_menu/oled_menu_context.h"
#include "oled_menu/oled_menu_input.h"
#include "oled_menu/oled_menu_render_output.h"

namespace soarm {

namespace {

using Profile = ControllerOperationProfile;

} // namespace

bool LeaderApp::shouldShowInteractiveOledMenu() const {
  if (wifiOta_.isOtaInProgress()) {
    return false;
  }

  const Profile profile = sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  if (profile == Profile::OtaReady) {
    return false;
  }

  if (profile == Profile::TeleopWifi) {
    return false;
  }

  if (profile == Profile::CalibrationLeader || profile == Profile::CalibrationFollower) {
    return false;
  }

  if (teleopContinuousEnabled_.load() || passthroughEngaged_.load() || calibrationEngaged_.load()) {
    return false;
  }

  const ArmRole calRole = activeCalibrationRole();
  CalibrationOledInput calInput{};
  calInput.profile = profile;
  calInput.calibrationPhase = calibrationPhase_.load();
  calInput.calibrationEngaged = calibrationEngaged_.load();
  calInput.followerCenterPending = followerCalibrationCenterPending_.load();
  calInput.activeRole = calRole;
  if (calibrationOledWorkflow_.resolve(calInput) != CalibrationOledScreen::Inactive) {
    return false;
  }

  return true;
}

void LeaderApp::buildOledMenuContext(OledMenuContext &context) const {
  context.leaderIp = wifiOta_.ipAddress();
  context.followerIpHint = followerIpHint_;
  context.pairedPeerMac = presenceService_->pairedPeerMac();
  context.espNowLinked = presenceService_->isFollowerLinked();
  context.espNowPaired = presenceService_->isPaired();
  context.xboxBlePaired = xboxControllerService_.isControllerPaired();
  context.telemetryListening = telemetryStreamServer_.isDashboardListening();
  context.leaderServoCount = servoBusService_.lastScanCount();
  context.followerServoCount = presenceService_->followerServoCount();
}

void LeaderApp::handleInteractiveOledMenuInput() {
  bool menuChanged = false;

  if (xboxControllerService_.consumeModeCycleRequest()) {
    if (oledMenuNavigator_.isAtRoot()) {
      menuChanged = oledMenuNavigator_.onInput(OledMenuInputEvent::ModeDown);
    }
  }

  int8_t dpadDirection = 0;
  while (xboxControllerService_.consumeDpadVerticalStep(dpadDirection)) {
    const OledMenuInputEvent event =
        dpadDirection > 0 ? OledMenuInputEvent::NavigateUp : OledMenuInputEvent::NavigateDown;
    menuChanged = oledMenuNavigator_.onInput(event) || menuChanged;
  }

  if (xboxControllerService_.consumeButtonPress(XboxLogicalButton::A)) {
    menuChanged = oledMenuNavigator_.onInput(OledMenuInputEvent::Select) || menuChanged;
  }

  if (xboxControllerService_.consumeButtonPress(XboxLogicalButton::B)) {
    menuChanged = oledMenuNavigator_.onInput(OledMenuInputEvent::Back) || menuChanged;
  }

  if (menuChanged) {
    lastOledRefreshMs_ = 0U;
  }
}

void LeaderApp::refreshInteractiveOledMenu(uint32_t uptimeMs) {
  OledMenuContext context{};
  buildOledMenuContext(context);

  OledMenuRenderOutput output{};
  oledMenuNavigator_.render(context, output);
  oledMenu_.showNavigationMenu(output);
}

} // namespace soarm
