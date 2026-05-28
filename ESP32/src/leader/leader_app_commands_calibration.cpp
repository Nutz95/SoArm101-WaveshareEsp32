#include "leader_app.h"

#include "../Config/leader_runtime_config.h"
#include "../common/calibration/calibration_profile_utils.h"

#include <cstring>

namespace soarm {

namespace {

constexpr uint32_t kCalibrationCaptureMin = 0U;
constexpr uint32_t kCalibrationCaptureMax = 1U;
constexpr uint32_t kCalibrationConfirmCenter = 2U;
constexpr uint32_t kCalibrationFinish = 3U;
constexpr uint32_t kCalibrationCancel = 4U;

void resetWorkingProfile(CalibrationProfile &profile) {
  for (uint8_t servoIndex = 0U; servoIndex < CalibrationProfile::kServoCount; ++servoIndex) {
    profile.minPosition[servoIndex] = 4095U;
    profile.maxPosition[servoIndex] = 0U;
  }
}

bool expandWorkingProfileFromTelemetry(CalibrationProfile &profile, const char *telemetryText) {
  if (telemetryText == nullptr) {
    return false;
  }

  bool updated = false;
  const char *cursor = telemetryText;
  while (*cursor != '\0') {
    while (*cursor != '\0' && *cursor != '#') {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }

    unsigned int servoId = 0U;
    int position = 0;
    if (sscanf(cursor, "#%u p%d", &servoId, &position) == 2 &&
        servoId >= 1U && servoId <= CalibrationProfile::kServoCount) {
      const uint8_t servoIndex = static_cast<uint8_t>(servoId - 1U);
      uint16_t clamped = static_cast<uint16_t>(position);
      if (position < 0) {
        clamped = 0U;
      } else if (position > 4095) {
        clamped = 4095U;
      }
      if (clamped < profile.minPosition[servoIndex]) {
        profile.minPosition[servoIndex] = clamped;
      }
      if (clamped > profile.maxPosition[servoIndex]) {
        profile.maxPosition[servoIndex] = clamped;
      }
      updated = true;
    }

    while (*cursor != '\0' && *cursor != ';' && *cursor != '#') {
      ++cursor;
    }
    if (*cursor == ';') {
      ++cursor;
    }
  }

  return updated;
}

bool profileHasRange(const CalibrationProfile &profile) {
  for (uint8_t servoIndex = 0U; servoIndex < CalibrationProfile::kServoCount; ++servoIndex) {
    if (profile.maxPosition[servoIndex] >= profile.minPosition[servoIndex]) {
      return true;
    }
  }
  return false;
}

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

void LeaderApp::beginCalibrationRangeCapture() {
  calibrationPhase_.store(1U);
  releaseCalibrationTorqueForActiveRole();

  if (activeCalibrationRole() == ArmRole::Leader) {
    leaderCalibrationProfileBackup_ = leaderCalibrationProfile_;
    resetWorkingProfile(leaderCalibrationWorkingProfile_);
  } else {
    followerCalibrationProfileBackup_ = followerCalibrationProfile_;
    resetWorkingProfile(followerCalibrationWorkingProfile_);
  }

  (void)sampleCalibrationRangeCapture();
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
    beginCalibrationRangeCapture();

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