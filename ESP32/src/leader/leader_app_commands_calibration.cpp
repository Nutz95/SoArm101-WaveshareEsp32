#include "leader_app.h"
#include "leader_calibration_workflow_internal.h"

#include "../Config/leader_runtime_config.h"
#include "../common/calibration/calibration_profile_utils.h"

namespace soarm {

namespace {

constexpr uint32_t kCalibrationCaptureMin = 0U;
constexpr uint32_t kCalibrationCaptureMax = 1U;
constexpr uint32_t kCalibrationConfirmCenter = 2U;
constexpr uint32_t kCalibrationFinish = 3U;
constexpr uint32_t kCalibrationCancel = 4U;

} // namespace

ArmRole LeaderApp::activeCalibrationRole() const {
  return controllerOperationProfile_.load() == 1U ? ArmRole::Follower : ArmRole::Leader;
}

void LeaderApp::releaseCalibrationTorqueForActiveRole() {
  if (activeCalibrationRole() == ArmRole::Leader) {
    servoBusService_.setDebugManual(false);
    servoBusService_.setTorqueEnabledForDetectedServos(false);
    return;
  }

  if (!presenceService_->isFollowerLinked()) {
    return;
  }

  const uint16_t requestId = static_cast<uint16_t>(teleopContinuousRequestCounter_ + 1U);
  teleopContinuousRequestCounter_ = requestId;
  (void)presenceService_->requestServoControl(
      static_cast<uint8_t>(ServoControlOpcode::DebugDisable),
      0U,
      requestId);
}

bool LeaderApp::beginCalibrationRangeCapture() {
  const ArmRole role = activeCalibrationRole();
  if (role == ArmRole::Leader) {
    if (!servoBusService_.calibrateOffsetsForDetectedServos()) {
      return false;
    }
  } else {
    if (!presenceService_->isFollowerLinked()) {
      return false;
    }
    const uint16_t requestId = static_cast<uint16_t>(teleopContinuousRequestCounter_ + 1U);
    teleopContinuousRequestCounter_ = requestId;
    if (!presenceService_->requestServoControl(
            static_cast<uint8_t>(ServoControlOpcode::CalibrationCenter),
            0U,
            requestId)) {
      return false;
    }
  }

  calibrationPhase_.store(1U);
  releaseCalibrationTorqueForActiveRole();

  if (role == ArmRole::Leader) {
    leaderCalibrationProfileBackup_ = leaderCalibrationProfile_;
    resetWorkingProfile(leaderCalibrationWorkingProfile_);
  } else {
    followerCalibrationProfileBackup_ = followerCalibrationProfile_;
    resetWorkingProfile(followerCalibrationWorkingProfile_);
  }

  return role == ArmRole::Leader ? sampleCalibrationRangeCapture() : true;
}

bool LeaderApp::sampleCalibrationRangeCapture() {
  if (calibrationPhase_.load() == 0U) {
    return false;
  }

  if (activeCalibrationRole() == ArmRole::Leader) {
    servoBusService_.refreshKnownTelemetryFast();
    return expandWorkingProfileFromTelemetry(
        leaderCalibrationWorkingProfile_,
        servoBusService_.lastTelemetryText());
  }

  return expandWorkingProfileFromTelemetry(
      followerCalibrationWorkingProfile_,
      presenceService_->followerServoTelemetry());
}

bool LeaderApp::commitCalibrationRangeCapture() {
  (void)sampleCalibrationRangeCapture();

  const ArmRole role = activeCalibrationRole();
  if (role == ArmRole::Leader) {
    if (!profileHasRange(leaderCalibrationWorkingProfile_)) {
      return false;
    }
    leaderCalibrationProfile_ = leaderCalibrationWorkingProfile_;
    if (!calibrationStore_.save(ArmRole::Leader, leaderCalibrationProfile_)) {
      return false;
    }
  } else {
    if (!profileHasRange(followerCalibrationWorkingProfile_)) {
      return false;
    }
    followerCalibrationProfile_ = followerCalibrationWorkingProfile_;
    if (!calibrationStore_.save(ArmRole::Follower, followerCalibrationProfile_)) {
      return false;
    }

    if (presenceService_->isFollowerLinked()) {
      const uint16_t requestId = static_cast<uint16_t>(teleopContinuousRequestCounter_ + 1U);
      teleopContinuousRequestCounter_ = requestId;
      (void)presenceService_->requestServoControl(
          static_cast<uint8_t>(ServoControlOpcode::CalibrationCapture),
          1U,
          requestId);
    }
  }

  calibrationPhase_.store(0U);
  releaseCalibrationTorqueForActiveRole();
  return true;
}

void LeaderApp::cancelCalibrationRangeCapture() {
  const ArmRole role = activeCalibrationRole();
  if (role == ArmRole::Leader) {
    leaderCalibrationProfile_ = leaderCalibrationProfileBackup_;
  } else {
    followerCalibrationProfile_ = followerCalibrationProfileBackup_;
  }
  calibrationPhase_.store(0U);
  releaseCalibrationTorqueForActiveRole();
}

bool LeaderApp::handleTeleopCalibrationCaptureValueCommand() {
  uint32_t value = 0U;
  uint16_t requestId = 0U;
  if (!telemetryStreamServer_.consumeTeleopCalibrationCaptureRequested(value, requestId)) {
    return false;
  }

  beginCommandTracking(requestId, static_cast<uint8_t>(LeaderCommandAction::TeleopCalibrationCapture));
  handleTeleopCalibrationCaptureCommand(value, requestId);
  return true;
}

void LeaderApp::handleTeleopCalibrationCaptureCommand(uint32_t value, uint16_t requestId) {
  if (value == kCalibrationConfirmCenter) {
    if (!beginCalibrationRangeCapture()) {
      setLeaderCommandStatus(CommandAckStatus::Rejected);
      setFollowerCommandStatus(CommandAckStatus::Rejected);
      setTransientStatus("cal center failed", config::leader::kMoveStatusHoldMs);
      return;
    }

    setLeaderCommandStatus(CommandAckStatus::Applied);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus(
        activeCalibrationRole() == ArmRole::Leader ? "cal leader move extremes" : "cal follower move extremes",
        config::leader::kMoveStatusHoldMs);
    return;
  }

  if (value == kCalibrationFinish) {
    if (!commitCalibrationRangeCapture()) {
      setLeaderCommandStatus(CommandAckStatus::Rejected);
      setFollowerCommandStatus(CommandAckStatus::Rejected);
      setTransientStatus("cal telemetry missing", config::leader::kMoveStatusHoldMs);
      return;
    }
    applyControllerOperationProfile(2U);
    setLeaderCommandStatus(CommandAckStatus::Applied);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("calibration validated", config::leader::kMoveStatusHoldMs);
    return;
  }

  if (value == kCalibrationCancel) {
    cancelCalibrationRangeCapture();
    applyControllerOperationProfile(2U); // return to teleop_espnow so the user is never stuck in cal mode
    setLeaderCommandStatus(CommandAckStatus::Applied);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("calibration canceled", config::leader::kMoveStatusHoldMs);
    return;
  }

  (void)requestId;
  const bool sampleOk = sampleCalibrationRangeCapture();
  setLeaderCommandStatus(sampleOk ? CommandAckStatus::Applied : CommandAckStatus::Rejected);
  setFollowerCommandStatus(CommandAckStatus::None);
  setTransientStatus(
      value == kCalibrationCaptureMin ? "calibration sample min" : "calibration sample max",
      config::leader::kMoveStatusHoldMs);
}

} // namespace soarm