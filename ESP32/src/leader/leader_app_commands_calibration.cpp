#include "leader_app.h"
#include "leader_calibration_workflow_internal.h"
#include "leader_presence_service.h"

#include "../Config/leader_runtime_config.h"
#include "../common/calibration/calibration_profile_utils.h"
#include "../common/controller/controller_operation_profile.h"

namespace soarm {

namespace {

constexpr uint32_t kCalibrationCaptureMin = 0U;
constexpr uint32_t kCalibrationCaptureMax = 1U;
constexpr uint32_t kCalibrationConfirmCenter = 2U;
constexpr uint32_t kCalibrationFinish = 3U;
constexpr uint32_t kCalibrationCancel = 4U;

} // namespace

ArmRole LeaderApp::activeCalibrationRole() const {
  return sanitizeControllerOperationProfile(controllerOperationProfile_.load()) ==
                 ControllerOperationProfile::CalibrationFollower
             ? ArmRole::Follower
             : ArmRole::Leader;
}

void LeaderApp::enterCalibrationRangePhase(ArmRole role) {
  calibrationPhase_.store(1U);
  if (role == ArmRole::Leader) {
    leaderCalibrationProfileBackup_ = leaderCalibrationProfile_;
    resetWorkingProfile(leaderCalibrationWorkingProfile_);
  } else {
    followerCalibrationProfileBackup_ = followerCalibrationProfile_;
    resetWorkingProfile(followerCalibrationWorkingProfile_);
  }
  releaseCalibrationTorqueForActiveRole();
}

void LeaderApp::releaseCalibrationTorqueForActiveRole() {
  if (activeCalibrationRole() == ArmRole::Leader) {
    servoBusService_.setDebugManual(false);
    servoBusService_.setTorqueEnabledForDetectedServos(false);
    return;
  }

  auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get());
  if (presence == nullptr || !presence->canCommandPairedFollower()) {
    return;
  }

  const uint16_t requestId = static_cast<uint16_t>(teleopContinuousRequestCounter_ + 1U);
  teleopContinuousRequestCounter_ = requestId;
  (void)presenceService_->requestServoControl(
      static_cast<uint8_t>(ServoControlOpcode::DebugDisable),
      0U,
      requestId);
}

bool LeaderApp::applyCalibrationCenter() {
  if (!calibrationEngaged_.load() || calibrationPhase_.load() != 0U ||
      followerCalibrationCenterPending_.load()) {
    return false;
  }

  const ArmRole role = activeCalibrationRole();
  if (role == ArmRole::Leader) {
    if (!servoBusService_.calibrateOffsetsForDetectedServos()) {
      return false;
    }

    enterCalibrationRangePhase(ArmRole::Leader);
    (void)sampleCalibrationRangeCapture();
    return true;
  }

  auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get());
  if (presence == nullptr || !presence->canCommandPairedFollower()) {
    return false;
  }

  presenceService_->refreshFollowerLinkGrace();

  const uint16_t requestId = static_cast<uint16_t>(teleopContinuousRequestCounter_ + 1U);
  teleopContinuousRequestCounter_ = requestId;
  if (!presenceService_->requestServoControl(
          static_cast<uint8_t>(ServoControlOpcode::CalibrationCenter),
          0U,
          requestId)) {
    return false;
  }

  followerCalibrationCenterRequestId_ = requestId;
  followerCalibrationCenterStartedMs_ = millis();
  followerCalibrationCenterLastResendMs_ = followerCalibrationCenterStartedMs_;
  followerCalibrationCenterResendRemaining_ = config::leader::kFollowerCalibrationCenterMaxResends;
  followerCalibrationCenterPending_.store(true);
  return true;
}

void LeaderApp::pollFollowerCalibrationCenterAck(uint32_t nowMs) {
  if (!followerCalibrationCenterPending_.load()) {
    return;
  }

  const bool requestIdMatch =
      presenceService_->followerLastAckRequestId() == followerCalibrationCenterRequestId_;
  const bool opMatch = presenceService_->followerLastAckCommandOp() ==
                       static_cast<uint8_t>(ServoControlOpcode::CalibrationCenter);
  if (requestIdMatch && opMatch) {
    const CommandAckStatus status =
        static_cast<CommandAckStatus>(presenceService_->followerLastAckStatus());
    if (status == CommandAckStatus::Applied) {
      followerCalibrationCenterPending_.store(false);
      followerCalibrationCenterResendRemaining_ = 0U;
      enterCalibrationRangePhase(ArmRole::Follower);
      presenceService_->refreshFollowerLinkGrace();
      setTransientStatus("cal follower move extremes", config::leader::kMoveStatusHoldMs);
    } else if (status == CommandAckStatus::Failed || status == CommandAckStatus::Rejected) {
      followerCalibrationCenterPending_.store(false);
      followerCalibrationCenterResendRemaining_ = 0U;
      setTransientStatus("cal follower center fail", config::leader::kMoveStatusHoldMs);
    }
    return;
  }

  if (followerCalibrationCenterResendRemaining_ > 0U &&
      (nowMs - followerCalibrationCenterLastResendMs_) >= config::leader::kFollowerCalibrationCenterResendMs) {
    followerCalibrationCenterLastResendMs_ = nowMs;
    --followerCalibrationCenterResendRemaining_;
    (void)presenceService_->requestServoControl(
        static_cast<uint8_t>(ServoControlOpcode::CalibrationCenter),
        0U,
        followerCalibrationCenterRequestId_);
  }

  if ((nowMs - followerCalibrationCenterStartedMs_) >= config::leader::kFollowerCalibrationCenterAckTimeoutMs) {
    followerCalibrationCenterPending_.store(false);
    followerCalibrationCenterResendRemaining_ = 0U;
    setTransientStatus("cal follower center timeout", config::leader::kMoveStatusHoldMs);
  }
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

  presenceService_->refreshFollowerLinkGrace();
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

    auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get());
    if (presence != nullptr && presence->canCommandPairedFollower()) {
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
  if (role == ArmRole::Follower) {
    nudgeFollowerLinkAfterCalibration();
  }
  return true;
}

void LeaderApp::cancelCalibrationRangeCapture() {
  followerCalibrationCenterPending_.store(false);
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
  if (!calibrationEngaged_.load()) {
    rejectCalibrationCommandNotStarted();
    return;
  }

  if (value == kCalibrationConfirmCenter) {
    handleCalibrationConfirmCenterCommand();
    return;
  }

  if (value == kCalibrationFinish) {
    handleCalibrationFinishCommand();
    return;
  }

  if (value == kCalibrationCancel) {
    handleCalibrationCancelCommand();
    return;
  }

  (void)requestId;
  handleCalibrationSampleCommand(value);
}

void LeaderApp::rejectCalibrationCommandNotStarted() {
  setLeaderCommandStatus(CommandAckStatus::Rejected);
  setFollowerCommandStatus(CommandAckStatus::Rejected);
  setTransientStatus("calibration not started", config::leader::kMoveStatusHoldMs);
}

void LeaderApp::handleCalibrationConfirmCenterCommand() {
  if (calibrationPhase_.load() != 0U || followerCalibrationCenterPending_.load()) {
    setLeaderCommandStatus(CommandAckStatus::Rejected);
    setFollowerCommandStatus(CommandAckStatus::Rejected);
    return;
  }

  if (!applyCalibrationCenter()) {
    setLeaderCommandStatus(CommandAckStatus::Rejected);
    setFollowerCommandStatus(CommandAckStatus::Rejected);
    setTransientStatus("cal center failed", config::leader::kMoveStatusHoldMs);
    return;
  }

  setLeaderCommandStatus(CommandAckStatus::Applied);
  setFollowerCommandStatus(CommandAckStatus::None);
  setTransientStatus(
      activeCalibrationRole() == ArmRole::Leader ? "cal leader centering" : "cal follower center",
      config::leader::kMoveStatusHoldMs);
}

void LeaderApp::handleCalibrationFinishCommand() {
  const ArmRole role = activeCalibrationRole();
  if (!commitCalibrationRangeCapture()) {
    setLeaderCommandStatus(CommandAckStatus::Rejected);
    setFollowerCommandStatus(CommandAckStatus::Rejected);
    setTransientStatus("cal telemetry missing", config::leader::kMoveStatusHoldMs);
    return;
  }
  calibrationOledWorkflow_.showCommittedResult(role, millis());
  calibrationEngaged_.store(false);
  applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::TeleopEspNow));
  setLeaderCommandStatus(CommandAckStatus::Applied);
  setFollowerCommandStatus(CommandAckStatus::None);
  setTransientStatus("calibration validated", config::leader::kMoveStatusHoldMs);
}

void LeaderApp::handleCalibrationCancelCommand() {
  const ArmRole role = activeCalibrationRole();
  calibrationOledWorkflow_.showCanceledResult(role, millis());
  applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::TeleopEspNow));
  setLeaderCommandStatus(CommandAckStatus::Applied);
  setFollowerCommandStatus(CommandAckStatus::None);
  setTransientStatus("calibration canceled", config::leader::kMoveStatusHoldMs);
}

void LeaderApp::handleCalibrationSampleCommand(uint32_t value) {
  const bool sampleOk = sampleCalibrationRangeCapture();
  setLeaderCommandStatus(sampleOk ? CommandAckStatus::Applied : CommandAckStatus::Rejected);
  setFollowerCommandStatus(CommandAckStatus::None);
  const char *status = value == kCalibrationCaptureMin ? "calibration sample min"
                                                        : "calibration sample max";
  setTransientStatus(status, config::leader::kMoveStatusHoldMs);
}

} // namespace soarm
