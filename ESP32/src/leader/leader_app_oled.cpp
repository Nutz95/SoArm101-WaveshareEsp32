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

  if (profile == ControllerOperationProfile::OtaReady) {
    if (!otaEngaged_.load()) {
      oledMenu_.showOtaAwaitEnter(wifiOta_.ipAddress());
    } else {
      oledMenu_.showOtaActive(wifiOta_.ipAddress());
    }
    return;
  }

  if (profile == ControllerOperationProfile::TeleopWifi) {
    if (!wifiDirectLinkEngaged_.load()) {
      oledMenu_.showWifiDirectAwaitEnter(wifiOta_.ipAddress(), followerIpHint_);
      return;
    }
    if (!wifiDirectSession_.isFollowerReady()) {
      oledMenu_.showWifiDirectWaitingFollower(wifiDirectSession_.leaderApIp());
      return;
    }
    if (!wifiDirectTeleopActive_.load()) {
      oledMenu_.showWifiDirectAwaitStart(
          wifiDirectSession_.leaderApIp(), wifiDirectSession_.followerTeleopIp());
      return;
    }
  }

  const ArmRole calRole = activeCalibrationRole();
  CalibrationOledInput calInput{};
  calInput.profile = profile;
  calInput.calibrationPhase = calibrationPhase_.load();
  calInput.calibrationEngaged = calibrationEngaged_.load();
  calInput.followerCenterPending = followerCalibrationCenterPending_.load();
  calInput.activeRole = calRole;
  calInput.nowMs = uptimeMs;
  calInput.centerConfirmArmedAtMs = calibrationCenterConfirmArmedAtMs_;
  if (calRole == ArmRole::Leader) {
    calInput.workingProfile = &leaderCalibrationWorkingProfile_;
    calInput.liveTelemetry = servoBusService_.lastTelemetryText();
  } else {
    calInput.workingProfile = &followerCalibrationWorkingProfile_;
    calInput.liveTelemetry = presenceService_->followerServoTelemetry();
  }

  if (profile == ControllerOperationProfile::CalibrationLeader ||
      profile == ControllerOperationProfile::CalibrationFollower) {
    const CalibrationOledScreen calScreen = calibrationOledWorkflow_.resolve(calInput);
    const char *resultText = calibrationOledWorkflow_.resultBannerText(uptimeMs);
    oledMenu_.showCalibration(calScreen, calInput, resultText);
    return;
  }

  const CalibrationOledScreen calScreen = calibrationOledWorkflow_.resolve(calInput);
  if (calScreen != CalibrationOledScreen::Inactive) {
    const char *resultText = calibrationOledWorkflow_.resultBannerText(uptimeMs);
    oledMenu_.showCalibration(calScreen, calInput, resultText);
    return;
  }

  const char *leaderIp = wifiOta_.ipAddress();
  const char *followerIp = followerIpHint_;
  if (profile == ControllerOperationProfile::TeleopWifi && wifiDirectTeleopActive_.load()) {
    leaderIp = wifiDirectSession_.leaderApIp();
    followerIp = wifiDirectSession_.followerTeleopIp();
  }

  oledMenu_.showDashboard(
      leaderIp,
      followerIp,
      mode_,
      static_cast<TeleopTransportMode>(teleopTransportMode_.load()),
      statusLine_,
      uptimeMs);
}

} // namespace soarm
