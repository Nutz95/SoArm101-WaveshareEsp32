#include "leader_app.h"

#include "../Config/leader_runtime_config.h"
#include "../common/controller/controller_operation_profile.h"

#include <cstdio>
#include <cstring>

namespace soarm {

namespace {

bool shouldHoldTeleopRuntime(
    ControllerOperationProfile profile,
    bool teleopContinuousEnabled,
    bool espNowLinked,
    bool wifiDirectTeleopActive) {
  if (!teleopContinuousEnabled || !espNowLinked) {
    return false;
  }
  if (profile == ControllerOperationProfile::TeleopEspNow) {
    return true;
  }
  if (profile == ControllerOperationProfile::TeleopEspNowTurbo) {
    return true;
  }
  if (profile == ControllerOperationProfile::TeleopEspNowFeedback) {
    return true;
  }
  return profile == ControllerOperationProfile::TeleopWifi && wifiDirectTeleopActive;
}

} // namespace

void LeaderApp::computeModeAndStatus() {
  const bool followerIpValid = presenceService_->hasValidFollowerIp();
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  const bool rangeCaptureActive = calibrationPhase_.load() != 0U;
  const bool calibrationProfileSelected =
      profile == ControllerOperationProfile::CalibrationLeader ||
      profile == ControllerOperationProfile::CalibrationFollower;
  const bool calibrationProfileActive = calibrationProfileSelected && calibrationEngaged_.load();

  if (applyOtaModeAndStatus(profile)) {
    return;
  }

  if (applyPassthroughModeAndStatus(profile)) {
    return;
  }

  if (applyHeldCommandModeAndStatus(profile, calibrationProfileSelected, calibrationProfileActive)) {
    return;
  }

  if (applyServoFaultModeAndStatus()) {
    return;
  }

  applyRuntimeModeAndStatus(profile, followerIpValid, rangeCaptureActive, calibrationProfileSelected);
  statusLine_[sizeof(statusLine_) - 1] = '\0';
}

bool LeaderApp::applyOtaModeAndStatus(ControllerOperationProfile profile) {
  if (profile != ControllerOperationProfile::OtaReady) {
    return false;
  }

  mode_ = OperationMode::Idle;
  if (!otaEngaged_.load()) {
    strncpy(statusLine_, "OTA? press A", sizeof(statusLine_) - 1);
  } else if (wifiOta_.isConnected()) {
    strncpy(statusLine_, "OTA active: flash", sizeof(statusLine_) - 1);
  } else if (wifiOta_.isStaConnectDesired()) {
    strncpy(statusLine_, "OTA: wifi connect", sizeof(statusLine_) - 1);
  } else {
    strncpy(statusLine_, "OTA: wifi off", sizeof(statusLine_) - 1);
  }
  statusLine_[sizeof(statusLine_) - 1] = '\0';
  return true;
}

bool LeaderApp::applyPassthroughModeAndStatus(ControllerOperationProfile profile) {
  if (profile != ControllerOperationProfile::Passthrough) {
    return false;
  }

  if (passthroughEngaged_.load()) {
    mode_ = OperationMode::Passthrough;
    strncpy(statusLine_, "passthrough usb 1M", sizeof(statusLine_) - 1);
  } else {
    mode_ = OperationMode::Idle;
    strncpy(statusLine_, "passthrough? press A", sizeof(statusLine_) - 1);
  }
  statusLine_[sizeof(statusLine_) - 1] = '\0';
  return true;
}

bool LeaderApp::applyHeldCommandModeAndStatus(
    ControllerOperationProfile profile,
    bool calibrationProfileSelected,
    bool calibrationProfileActive) {
  if (millis() >= commandStatusHoldUntilMs_) {
    return false;
  }

  if (profile == ControllerOperationProfile::OtaReady && !otaEngaged_.load()) {
    mode_ = OperationMode::Idle;
    strncpy(statusLine_, "OTA? press A", sizeof(statusLine_) - 1);
  } else if (profile == ControllerOperationProfile::TeleopWifi && !wifiDirectLinkEngaged_.load()) {
    mode_ = OperationMode::Idle;
    setWifiDirectPreviewStatus();
  } else if (calibrationProfileSelected && !calibrationEngaged_.load()) {
    mode_ = OperationMode::Idle;
    setCalibrationPreviewStatus(profile);
  } else if (calibrationProfileActive || !localInputs_.calibrationDone) {
    mode_ = OperationMode::CalibrationLeader;
  } else {
    mode_ = localInputs_.espNowLinked ? OperationMode::Teleoperation : OperationMode::Idle;
  }
  return true;
}

bool LeaderApp::applyServoFaultModeAndStatus() {
  if (!leaderServoFault_ && !followerServoFault_) {
    return false;
  }

  if (teleopContinuousEnabled_.load()) {
    return false;
  }

  mode_ = localInputs_.espNowLinked ? OperationMode::Teleoperation : OperationMode::Idle;
  snprintf(
      statusLine_,
      sizeof(statusLine_),
      "servo cnt L%u F%u",
      static_cast<unsigned>(servoBusService_.lastScanCount()),
      static_cast<unsigned>(presenceService_->followerServoCount()));
  statusLine_[sizeof(statusLine_) - 1] = '\0';
  return true;
}

void LeaderApp::applyRuntimeModeAndStatus(
    ControllerOperationProfile profile,
    bool followerIpValid,
    bool rangeCaptureActive,
    bool calibrationProfileSelected) {
  const bool holdTeleopRuntime = shouldHoldTeleopRuntime(
      profile,
      teleopContinuousEnabled_.load(),
      localInputs_.espNowLinked,
      wifiDirectTeleopActive_.load());

  if (!localInputs_.joystickPaired) {
    if (holdTeleopRuntime) {
      mode_ = OperationMode::Teleoperation;
      strncpy(statusLine_, "teleop (no pad)", sizeof(statusLine_) - 1);
      return;
    }
    mode_ = OperationMode::Idle;
    strncpy(statusLine_, "pair joystick", sizeof(statusLine_) - 1);
  } else if (profile == ControllerOperationProfile::CalibrationFollower &&
             calibrationEngaged_.load()) {
    if (followerCalibrationCenterPending_.load()) {
      mode_ = OperationMode::CalibrationFollower;
      strncpy(statusLine_, "cal follower center", sizeof(statusLine_) - 1);
    } else if (localInputs_.espNowLinked) {
      mode_ = OperationMode::CalibrationFollower;
      strncpy(
          statusLine_,
          rangeCaptureActive ? "cal follower range" : "cal follower center",
          sizeof(statusLine_) - 1);
    } else if (presenceService_->isFollowerAvailable()) {
      mode_ = OperationMode::CalibrationFollower;
      strncpy(statusLine_, "follower link stale", sizeof(statusLine_) - 1);
    } else {
      mode_ = OperationMode::Idle;
      strncpy(statusLine_, "follower offline", sizeof(statusLine_) - 1);
    }
  } else if (profile == ControllerOperationProfile::TeleopWifi && !wifiDirectLinkEngaged_.load()) {
    mode_ = OperationMode::Idle;
    setWifiDirectPreviewStatus();
  } else if (profile == ControllerOperationProfile::TeleopWifi && wifiDirectLinkEngaged_.load() &&
             !wifiDirectSession_.isFollowerReady()) {
    mode_ = OperationMode::Teleoperation;
    strncpy(statusLine_, "wifi: wait follower", sizeof(statusLine_) - 1);
  } else if (profile == ControllerOperationProfile::TeleopWifi && !wifiDirectTeleopActive_.load()) {
    mode_ = OperationMode::Teleoperation;
    strncpy(statusLine_, "wifi: start? press A", sizeof(statusLine_) - 1);
  } else if (calibrationProfileSelected && !calibrationEngaged_.load()) {
    mode_ = OperationMode::Idle;
    setCalibrationPreviewStatus(profile);
  } else if (profile == ControllerOperationProfile::CalibrationLeader && calibrationEngaged_.load()) {
    mode_ = OperationMode::CalibrationLeader;
    strncpy(
        statusLine_,
        rangeCaptureActive ? "cal leader range" : "cal leader center",
        sizeof(statusLine_) - 1);
  } else if (!localInputs_.calibrationDone) {
    mode_ = OperationMode::CalibrationLeader;
    strncpy(statusLine_, "calibration required", sizeof(statusLine_) - 1);
  } else if (!localInputs_.espNowLinked) {
    mode_ = OperationMode::Idle;
    if (presenceService_->isFollowerAvailable()) {
      strncpy(statusLine_, "follower link stale", sizeof(statusLine_) - 1);
    } else if (presenceService_->followerIp()[0] != '\0' && !followerIpValid) {
      strncpy(statusLine_, "follower wifi down", sizeof(statusLine_) - 1);
    } else {
      strncpy(statusLine_, "follower offline", sizeof(statusLine_) - 1);
    }
  } else {
    mode_ = OperationMode::Teleoperation;
    const bool espNowTurboStyleMirror =
        usesEspNowTurboDownlinkProfile(profile) && teleopContinuousEnabled_.load();
    if (!espNowTurboStyleMirror) {
      strncpy(statusLine_, "teleop ready", sizeof(statusLine_) - 1);
    }
  }
}

void LeaderApp::setWifiDirectPreviewStatus() {
  strncpy(statusLine_, "wifi? press A", sizeof(statusLine_) - 1);
  statusLine_[sizeof(statusLine_) - 1] = '\0';
}

void LeaderApp::setCalibrationPreviewStatus(ControllerOperationProfile profile) {
  const char *status = profile == ControllerOperationProfile::CalibrationFollower
                           ? "cal follower? press A"
                           : "cal leader? press A";
  strncpy(statusLine_, status, sizeof(statusLine_) - 1);
  statusLine_[sizeof(statusLine_) - 1] = '\0';
}

void LeaderApp::updateFollowerState() {
  if (localInputs_.espNowLinked) {
    followerState_ = followerServoFault_ ? ArmRuntimeState::ServoFault : ArmRuntimeState::Ready;
    strncpy(followerIpHint_, presenceService_->followerIp(), sizeof(followerIpHint_) - 1);
    followerIpHint_[sizeof(followerIpHint_) - 1] = '\0';
  } else {
    followerState_ = ArmRuntimeState::WaitingEspNow;
    if (presenceService_->hasValidFollowerIp()) {
      strncpy(followerIpHint_, presenceService_->followerIp(), sizeof(followerIpHint_) - 1);
      followerIpHint_[sizeof(followerIpHint_) - 1] = '\0';
    } else {
      strncpy(followerIpHint_, "0.0.0.0", sizeof(followerIpHint_) - 1);
      followerIpHint_[sizeof(followerIpHint_) - 1] = '\0';
    }
  }
}

void LeaderApp::renderStatusLeds() {
  ArmRuntimeState localState = stateMachine_.computeState(localInputs_);
  if (leaderServoFault_) {
    localState = ArmRuntimeState::ServoFault;
  }

  ArmRuntimeState followerLedState = followerState_;
  if (followerServoFault_) {
    followerLedState = ArmRuntimeState::ServoFault;
  }

  if (mode_ == OperationMode::CalibrationLeader) {
    statusLedService_.render(0, ArmRuntimeState::WaitingCalibration);
    if (STATUS_LED_COUNT > 1U) {
      statusLedService_.render(1, followerLedState);
    }
    return;
  }

  if (mode_ == OperationMode::CalibrationFollower) {
    statusLedService_.render(0, localState);
    if (STATUS_LED_COUNT > 1U) {
      statusLedService_.render(1, ArmRuntimeState::WaitingCalibration);
    }
    return;
  }

  statusLedService_.render(0, localState);
  if (STATUS_LED_COUNT > 1U) {
    statusLedService_.render(1, followerLedState);
  }
}

void LeaderApp::updateTurboOledStatus(uint32_t nowMs) {
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  const bool turboMirrorActive =
      profile == ControllerOperationProfile::TeleopEspNowTurbo &&
      teleopContinuousEnabled_.load() &&
      mode_ == OperationMode::Teleoperation &&
      localInputs_.espNowLinked;

  if (!turboMirrorActive) {
    lastTurboOledStatusMs_ = 0U;
    return;
  }

  if (nowMs < commandStatusHoldUntilMs_) {
    return;
  }

  if (lastTurboOledStatusMs_ != 0U &&
      (nowMs - lastTurboOledStatusMs_) < config::leader::kTurboOledStatusPeriodMs) {
    return;
  }
  lastTurboOledStatusMs_ = nowMs;

  const uint8_t loopMs = teleopMirrorLatencyMetrics_.loopEwmaMs.load();
  const uint8_t drops = teleopMirrorLatencyMetrics_.sendFailCount.load();
  if (loopMs == 0U) {
    snprintf(statusLine_, sizeof(statusLine_), "turbo warmup");
  } else {
    snprintf(
        statusLine_,
        sizeof(statusLine_),
        "lat %ums dr%u",
        static_cast<unsigned>(loopMs),
        static_cast<unsigned>(drops));
  }
  statusLine_[sizeof(statusLine_) - 1] = '\0';
}

} // namespace soarm
