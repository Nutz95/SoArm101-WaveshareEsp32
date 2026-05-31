#include "leader_app.h"

#include "../Config/common_runtime_config.h"
#include "../common/controller/controller_operation_profile.h"

#include <cstdio>
#include <cstring>

namespace soarm {

void LeaderApp::computeModeAndStatus() {
  const bool followerIpValid = presenceService_->hasValidFollowerIp();
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  const bool rangeCaptureActive = calibrationPhase_.load() != 0U;
  const bool calibrationProfileActive =
      profile == ControllerOperationProfile::CalibrationLeader ||
      profile == ControllerOperationProfile::CalibrationFollower;

  if (profile == ControllerOperationProfile::Passthrough) {
    if (passthroughEngaged_.load()) {
      mode_ = OperationMode::Passthrough;
      strncpy(statusLine_, "passthrough usb 1M", sizeof(statusLine_) - 1);
    } else {
      mode_ = OperationMode::Idle;
      strncpy(statusLine_, "passthrough? press A", sizeof(statusLine_) - 1);
    }
    statusLine_[sizeof(statusLine_) - 1] = '\0';
    return;
  }

  if (millis() < commandStatusHoldUntilMs_) {
    if (profile == ControllerOperationProfile::CalibrationFollower && localInputs_.espNowLinked) {
      mode_ = OperationMode::CalibrationFollower;
    } else if (calibrationProfileActive || !localInputs_.calibrationDone) {
      mode_ = OperationMode::CalibrationLeader;
    } else {
      mode_ = localInputs_.espNowLinked ? OperationMode::Teleoperation : OperationMode::Idle;
    }
    return;
  }

  if (leaderServoFault_ || followerServoFault_) {
    mode_ = localInputs_.espNowLinked ? OperationMode::Teleoperation : OperationMode::Idle;
    snprintf(
        statusLine_,
        sizeof(statusLine_),
        "servo mismatch L:%u F:%u",
        static_cast<unsigned>(servoBusService_.lastScanCount()),
        static_cast<unsigned>(presenceService_->followerServoCount()));
    statusLine_[sizeof(statusLine_) - 1] = '\0';
    return;
  }

  if (!localInputs_.joystickPaired) {
    mode_ = OperationMode::Idle;
    strncpy(statusLine_, "pair joystick", sizeof(statusLine_) - 1);
  } else if (profile == ControllerOperationProfile::CalibrationFollower) {
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
  } else if (profile == ControllerOperationProfile::CalibrationLeader || !localInputs_.calibrationDone) {
    mode_ = OperationMode::CalibrationLeader;
    strncpy(
        statusLine_,
        rangeCaptureActive ? "cal leader range" : "cal leader center",
        sizeof(statusLine_) - 1);
  } else if (!localInputs_.espNowLinked) {
    mode_ = OperationMode::Idle;
    if (sanitizeControllerOperationProfile(controllerOperationProfile_.load()) ==
            ControllerOperationProfile::TeleopPcSerial &&
        teleopContinuousEnabled_.load()) {
      mode_ = OperationMode::Teleoperation;
      strncpy(statusLine_, "pc serial: COM bridge", sizeof(statusLine_) - 1);
    } else if (presenceService_->isFollowerAvailable()) {
      strncpy(statusLine_, "follower link stale", sizeof(statusLine_) - 1);
    } else if (presenceService_->followerIp()[0] != '\0' && !followerIpValid) {
      strncpy(statusLine_, "follower wifi down", sizeof(statusLine_) - 1);
    } else {
      strncpy(statusLine_, "follower offline", sizeof(statusLine_) - 1);
    }
  } else {
    mode_ = OperationMode::Teleoperation;
    if (sanitizeControllerOperationProfile(controllerOperationProfile_.load()) ==
        ControllerOperationProfile::TeleopPcSerial) {
      strncpy(statusLine_, "pc serial: COM bridge", sizeof(statusLine_) - 1);
    } else {
      strncpy(statusLine_, "teleop ready", sizeof(statusLine_) - 1);
    }
  }
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

} // namespace soarm
