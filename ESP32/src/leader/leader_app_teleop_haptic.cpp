#include "leader_app.h"

#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"
#include "../common/calibration/calibration_profile_utils.h"
#include "../common/controller/controller_operation_profile.h"
#include "../common/servo/servo_position_snapshot.h"
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

bool gripperLoadInEngageBand(uint8_t smoothedLoad) {
  return smoothedLoad >= config::leader::kTeleopHapticGripperEngageMinWireLoad &&
         smoothedLoad <= config::leader::kTeleopHapticGripperEngageMaxWireLoad;
}

} // namespace

void LeaderApp::resetTeleopHapticOverlay() {
  lastTeleopHapticMs_ = 0U;
  teleopHapticGripperEngaged_ = false;
  teleopHapticBusPrimed_ = false;
  teleopHapticGripperLoadEwma_ = 0U;
  teleopHapticEngageCandidateMs_ = 0U;
  teleopHapticDisengageCandidateMs_ = 0U;
  teleopHapticLastPosition_[kGripperSlot] = 0;
  teleopHapticHasLastPosition_[kGripperSlot] = false;
  teleopFeedbackGripperPresentPosValid_ = false;
  for (uint8_t i = 0U; i < config::common::kTeleopBatchMaxServos; ++i) {
    teleopFeedbackLoadsEwma_[i] = 0U;
  }
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

  const uint8_t sample = teleopFeedbackLoadsEwma_[kGripperSlot];
  const uint8_t previousSmoothed = teleopHapticGripperLoadEwma_;
  teleopHapticGripperLoadEwma_ =
      previousSmoothed == 0U
          ? sample
          : static_cast<uint8_t>((static_cast<uint16_t>(previousSmoothed) * 7U +
                                  static_cast<uint16_t>(sample) + 4U) /
                                 8U);

  ServoPositionSnapshot snapshot{};
  if (!servoBusService_.copyPositionSnapshot(snapshot)) {
    return;
  }

  int16_t leaderPresentPosition = 0;
  if (!findSnapshotPosition(snapshot, kGripperId, leaderPresentPosition)) {
    if (teleopHapticGripperEngaged_) {
      releaseGripperTorque(servoBusService_, teleopHapticLastPosition_[kGripperSlot]);
      teleopHapticGripperEngaged_ = false;
      teleopHapticEngageCandidateMs_ = 0U;
      teleopHapticDisengageCandidateMs_ = 0U;
    }
    return;
  }

  int16_t hapticGoalPosition = leaderPresentPosition;
  if (teleopFeedbackGripperPresentPosValid_) {
    hapticGoalPosition = remapServoPositionWithCalibration(
        kGripperId,
        teleopFeedbackGripperPresentPos_,
        followerCalibrationProfile_,
        leaderCalibrationProfile_);
  }

  bool leaderGripperMoving = false;
  if (teleopHapticHasLastPosition_[kGripperSlot]) {
    const int32_t delta = static_cast<int32_t>(leaderPresentPosition) -
                          static_cast<int32_t>(teleopHapticLastPosition_[kGripperSlot]);
    const int32_t absDelta = delta < 0 ? -delta : delta;
    leaderGripperMoving = absDelta >= config::leader::kTeleopHapticGripperMoveDelta;
  }
  teleopHapticLastPosition_[kGripperSlot] = leaderPresentPosition;
  teleopHapticHasLastPosition_[kGripperSlot] = true;

  if (leaderGripperMoving) {
    teleopHapticEngageCandidateMs_ = 0U;
    if (teleopHapticGripperEngaged_) {
      releaseGripperTorque(servoBusService_, leaderPresentPosition);
      teleopHapticGripperEngaged_ = false;
      teleopHapticDisengageCandidateMs_ = 0U;
    }
    return;
  }

  const uint8_t smoothedLoad = teleopHapticGripperLoadEwma_;
  uint8_t hapticLoad = smoothedLoad;
  if (teleopHapticGripperEngaged_ &&
      hapticLoad < config::leader::kTeleopHapticGripperEngageMinWireLoad) {
    // Hold minimum feedback while engaged — avoids drop-out when follower load plateaus.
    hapticLoad = config::leader::kTeleopHapticGripperEngageMinWireLoad;
  }

  if (!teleopHapticGripperEngaged_) {
    if (!gripperLoadInEngageBand(smoothedLoad)) {
      teleopHapticEngageCandidateMs_ = 0U;
      return;
    }
    if (teleopHapticEngageCandidateMs_ == 0U) {
      teleopHapticEngageCandidateMs_ = nowMs;
      return;
    }
    if ((nowMs - teleopHapticEngageCandidateMs_) < config::leader::kTeleopHapticEngageHoldMs) {
      return;
    }
    teleopHapticEngageCandidateMs_ = 0U;
    teleopHapticDisengageCandidateMs_ = 0U;
  } else {
    if (smoothedLoad > config::leader::kTeleopHapticGripperDisengageMaxWireLoad) {
      teleopHapticDisengageCandidateMs_ = 0U;
    } else {
      if (teleopHapticDisengageCandidateMs_ == 0U) {
        teleopHapticDisengageCandidateMs_ = nowMs;
        return;
      }
      if ((nowMs - teleopHapticDisengageCandidateMs_) < config::leader::kTeleopHapticDisengageHoldMs) {
        return;
      }
      releaseGripperTorque(servoBusService_, leaderPresentPosition);
      teleopHapticGripperEngaged_ = false;
      teleopHapticDisengageCandidateMs_ = 0U;
      return;
    }
  }

  const uint16_t torqueLimit = teleop_haptic::mapWireLoadToTorqueLimit(hapticLoad, true);
  const uint8_t id = kGripperId;
  const bool releaseTorque = false;
  if (servoBusService_.applyTeleopHapticFrame(&id, &hapticGoalPosition, &torqueLimit, &releaseTorque, 1U)) {
    teleopHapticGripperEngaged_ = true;
  }
}

} // namespace soarm
