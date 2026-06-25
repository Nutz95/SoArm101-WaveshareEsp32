#include "leader_app.h"

#include "../Config/leader_runtime_config.h"
#include "../common/controller/controller_operation_profile.h"
#include "../common/servo/servo_control_opcode.h"
#include "leader_presence_service.h"

namespace soarm {

void LeaderApp::prepareEspNowTeleopMirrorStart() {
  const uint32_t nowMs = millis();
  updateEspNowStaPrime(nowMs);

  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  if (isEspNowTeleopProfile(profile) && !homeStaChannelLearned_) {
    homeStaChannelLearned_ = true;
  }

  syncWifiRadioPolicyForProfile(profile);
  refreshEspNowRadioTransport();
}

void LeaderApp::releaseFollowerTeleopHold() {
  teleopContinuousEnabled_.store(false);
  teleopContinuousServoIdFilter_.store(0U);
  lastTurboOledStatusMs_ = 0U;
  servoDebugManual_ = false;
  servoBusService_.setDebugManual(false);
  servoBusService_.setTorqueEnabledForDetectedServos(false);

  if (!presenceService_->isFollowerLinked()) {
    deferHomeStaReconnect_.store(false);
    return;
  }

  deferHomeStaReconnect_.store(true);
  refreshEspNowRadioTransport();

  const uint16_t requestId = static_cast<uint16_t>(teleopContinuousRequestCounter_ + 1U);
  teleopContinuousRequestCounter_ = requestId;

  beginCommandTracking(requestId, static_cast<uint8_t>(ServoControlOpcode::DebugDisable));
  setLeaderCommandStatus(CommandAckStatus::None);
  const bool sent = presenceService_->requestServoControl(
      static_cast<uint8_t>(ServoControlOpcode::DebugDisable),
      0U,
      requestId);
  if (sent) {
    setFollowerCommandStatus(CommandAckStatus::Accepted);
    setFollowerRetryPayload(
        static_cast<uint8_t>(ServoControlOpcode::DebugDisable),
        0U,
        config::leader::kFollowerCommandMaxRetries);
    awaitFollowerAck(
        requestId,
        static_cast<uint8_t>(ServoControlOpcode::DebugDisable),
        config::leader::kFollowerDebugAckTimeoutMs);
  } else {
    setFollowerCommandStatus(CommandAckStatus::Failed);
    deferHomeStaReconnect_.store(false);
  }
}

void LeaderApp::clearDeferHomeStaReconnectIfDone() {
  if (!deferHomeStaReconnect_.load()) {
    return;
  }
  if (followerAckPending_) {
    return;
  }
  deferHomeStaReconnect_.store(false);
  syncWifiRadioPolicyForProfile(
      sanitizeControllerOperationProfile(controllerOperationProfile_.load()));
}

} // namespace soarm
