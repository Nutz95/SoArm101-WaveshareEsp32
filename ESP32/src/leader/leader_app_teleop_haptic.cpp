#include "leader_app.h"

#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"
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

} // namespace

void LeaderApp::resetTeleopHapticOverlay() {
  lastTeleopHapticMs_ = 0U;
  teleopHapticGripperEngaged_ = false;
  teleopHapticBusPrimed_ = false;
  teleopHapticLastPosition_[kGripperSlot] = 0;
  teleopHapticHasLastPosition_[kGripperSlot] = false;
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

  const uint8_t wireLoad = teleopFeedbackLoadsEwma_[kGripperSlot];
  const bool followerGripLoaded =
      wireLoad >= config::leader::kTeleopHapticGripperEngageMinWireLoad &&
      wireLoad <= config::leader::kTeleopHapticGripperEngageMaxWireLoad;

  ServoPositionSnapshot snapshot{};
  if (!servoBusService_.copyPositionSnapshot(snapshot)) {
    return;
  }

  int16_t position = 0;
  if (!findSnapshotPosition(snapshot, kGripperId, position)) {
    if (teleopHapticGripperEngaged_) {
      releaseGripperTorque(servoBusService_, teleopHapticLastPosition_[kGripperSlot]);
      teleopHapticGripperEngaged_ = false;
    }
    return;
  }

  bool moving = false;
  if (teleopHapticHasLastPosition_[kGripperSlot]) {
    const int32_t delta =
        static_cast<int32_t>(position) - static_cast<int32_t>(teleopHapticLastPosition_[kGripperSlot]);
    const int32_t absDelta = delta < 0 ? -delta : delta;
    moving = absDelta >= config::leader::kTeleopHapticGripperMoveDelta;
  }
  teleopHapticLastPosition_[kGripperSlot] = position;
  teleopHapticHasLastPosition_[kGripperSlot] = true;

  const bool shouldEngage = followerGripLoaded && !moving;

  if (!shouldEngage) {
    if (teleopHapticGripperEngaged_) {
      releaseGripperTorque(servoBusService_, position);
      teleopHapticGripperEngaged_ = false;
    }
    return;
  }

  const uint16_t torqueLimit = teleop_haptic::mapWireLoadToTorqueLimit(wireLoad, true);
  const uint8_t id = kGripperId;
  const bool releaseTorque = false;
  if (servoBusService_.applyTeleopHapticFrame(&id, &position, &torqueLimit, &releaseTorque, 1U)) {
    teleopHapticGripperEngaged_ = true;
  }
}

} // namespace soarm
