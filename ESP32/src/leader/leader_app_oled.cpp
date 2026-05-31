#include "leader_app.h"

#include "../common/controller/controller_operation_profile.h"
#include "../common/teleop/teleop_transport_mode.h"

#include <cstring>

namespace soarm {

void LeaderApp::refreshOled(uint32_t uptimeMs) {
  if ((uptimeMs - lastOledRefreshMs_) < oledConfig_.refreshPeriodMs) {
    return;
  }

  lastOledRefreshMs_ = uptimeMs;
  if (wifiOta_.isOtaInProgress()) {
    strncpy(statusLine_, "ota updating", sizeof(statusLine_) - 1);
    statusLine_[sizeof(statusLine_) - 1] = '\0';
    oled_.showOtaProgress(50);
    return;
  }

  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  const ArmRole calRole = activeCalibrationRole();
  CalibrationOledInput calInput{};
  calInput.profile = profile;
  calInput.calibrationPhase = calibrationPhase_.load();
  calInput.followerCenterPending = followerCalibrationCenterPending_.load();
  calInput.activeRole = calRole;
  calInput.nowMs = uptimeMs;
  if (calRole == ArmRole::Leader) {
    calInput.workingProfile = &leaderCalibrationWorkingProfile_;
    calInput.liveTelemetry = servoBusService_.lastTelemetryText();
  } else {
    calInput.workingProfile = &followerCalibrationWorkingProfile_;
    calInput.liveTelemetry = presenceService_->followerServoTelemetry();
  }

  const CalibrationOledScreen calScreen = calibrationOledWorkflow_.resolve(calInput);
  if (calScreen != CalibrationOledScreen::Inactive) {
    const char *resultText = calibrationOledWorkflow_.resultBannerText(uptimeMs);
    oledMenu_.showCalibration(calScreen, calInput, resultText);
    return;
  }

  oledMenu_.showDashboard(
      wifiOta_.ipAddress(),
      followerIpHint_,
      mode_,
      static_cast<TeleopTransportMode>(teleopTransportMode_.load()),
      statusLine_,
      uptimeMs);
}

} // namespace soarm
