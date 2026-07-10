#include "leader_app.h"

#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"
#include "../common/calibration/calibration_profile_utils.h"
#include "../common/controller/controller_operation_profile.h"
#include "../common/servo/servo_position_snapshot.h"
#include "../common/teleop/teleop_haptic_contact.h"
#include "../common/teleop/teleop_haptic_mapper.h"
#include "../common/types/operation_mode.h"

namespace soarm {

namespace {

constexpr uint8_t kGripperId = config::leader::kTeleopHapticGripperServoId;
constexpr uint8_t kGripperSlot = config::leader::kTeleopHapticGripperSlotIndex;

bool findSnapshotPosition(const ServoPositionSnapshot &snapshot, uint8_t id, int16_t &positionOut) {
  for (uint8_t i = 0U; i < snapshot.count; ++i) {
    if (snapshot.samples[i].id == id) {
      positionOut = snapshot.samples[i].position;
      return true;
    }
  }
  return false;
}

void releaseGripperTorque(ServoBusService &bus, int16_t position) {
  const uint8_t id = kGripperId;
  const bool releaseTorque = true;
  const uint16_t torqueLimit = 0U;
  (void)bus.applyTeleopHapticFrame(&id, &position, &torqueLimit, &releaseTorque, 1U);
}

} // namespace

void LeaderApp::resetTeleopHapticOverlay() {
  lastTeleopHapticMs_ = 0U;
  teleopHapticGripperEngaged_ = false;
  teleopHapticBusPrimed_ = false;
  teleopHapticEngageStreak_ = 0U;
  teleopHapticDisengageCandidateMs_ = 0U;
  teleopFeedbackGripperPresentPosValid_ = false;
  servoBusService_.setTorqueEnabledForDetectedServos(false);
}

void LeaderApp::applyTeleopHapticOverlay(uint32_t nowMs) {
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  if (!isEspNowTeleopFeedbackProfile(profile) || !teleopContinuousEnabled_.load() ||
      mode_ != OperationMode::Teleoperation) {
    return;
  }

  if (!teleopHapticBusPrimed_) {
    servoBusService_.setTorqueEnabledForDetectedServos(false);
    teleopHapticGripperEngaged_ = false;
    teleopHapticBusPrimed_ = true;
  }

  if (lastTeleopHapticMs_ != 0U &&
      (nowMs - lastTeleopHapticMs_) < config::leader::kTeleopHapticPeriodMs) {
    return;
  }
  lastTeleopHapticMs_ = nowMs;

  ServoPositionSnapshot snapshot{};
  if (!servoBusService_.copyPositionSnapshot(snapshot)) {
    return;
  }

  int16_t leaderPresentPosition = 0;
  if (!findSnapshotPosition(snapshot, kGripperId, leaderPresentPosition)) {
    if (teleopHapticGripperEngaged_) {
      releaseGripperTorque(servoBusService_, leaderPresentPosition);
      teleopHapticGripperEngaged_ = false;
      teleopHapticEngageStreak_ = 0U;
      teleopHapticDisengageCandidateMs_ = 0U;
    }
    return;
  }

  int16_t followerOnLeader = leaderPresentPosition;
  if (teleopFeedbackGripperPresentPosValid_) {
    followerOnLeader = remapServoPositionWithCalibration(
        kGripperId,
        teleopFeedbackGripperPresentPos_,
        followerCalibrationProfile_,
        leaderCalibrationProfile_);
  }

  const uint8_t gripperLoad = teleopFeedbackLoads_[kGripperSlot];
  const int16_t hapticGoalPosition = teleop_haptic::selectGripperHapticGoal(
      leaderPresentPosition, followerOnLeader, gripperLoad);
  const int32_t absPositionGap =
      teleop_haptic::gripperLeaderFollowerAbsGap(leaderPresentPosition, followerOnLeader);

  const bool contactCandidate =
      teleop_haptic::shouldEngageGripperHaptic(gripperLoad, absPositionGap);
  if (contactCandidate) {
    if (teleopHapticEngageStreak_ < 255U) {
      ++teleopHapticEngageStreak_;
    }
  } else {
    teleopHapticEngageStreak_ = 0U;
  }

  if (!teleopHapticGripperEngaged_) {
    if (!contactCandidate ||
        teleopHapticEngageStreak_ < config::leader::kTeleopHapticEngageStreakRequired) {
      return;
    }
    teleopHapticDisengageCandidateMs_ = 0U;
  } else if (teleop_haptic::shouldDisengageMirrorCatchUp(gripperLoad, absPositionGap)) {
    releaseGripperTorque(servoBusService_, leaderPresentPosition);
    teleopHapticGripperEngaged_ = false;
    teleopHapticEngageStreak_ = 0U;
    teleopHapticDisengageCandidateMs_ = 0U;
    return;
  } else if (gripperLoad <= config::leader::kTeleopHapticGripperDisengageMaxWireLoad) {
    if (teleopHapticDisengageCandidateMs_ == 0U) {
      teleopHapticDisengageCandidateMs_ = nowMs;
      return;
    }
    if ((nowMs - teleopHapticDisengageCandidateMs_) < config::leader::kTeleopHapticDisengageHoldMs) {
      return;
    }
    releaseGripperTorque(servoBusService_, leaderPresentPosition);
    teleopHapticGripperEngaged_ = false;
    teleopHapticEngageStreak_ = 0U;
    teleopHapticDisengageCandidateMs_ = 0U;
    return;
  } else {
    teleopHapticDisengageCandidateMs_ = 0U;
  }

  const uint16_t torqueLimit = teleop_haptic::mapWireLoadToTorqueLimit(gripperLoad, true);
  const uint8_t id = kGripperId;
  const bool releaseTorque = false;
  if (servoBusService_.applyTeleopHapticFrame(&id, &hapticGoalPosition, &torqueLimit, &releaseTorque, 1U)) {
    teleopHapticGripperEngaged_ = true;
  }
}

} // namespace soarm
