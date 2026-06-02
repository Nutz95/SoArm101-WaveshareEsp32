#include "leader_app.h"

#include "../Config/leader_runtime_config.h"

namespace soarm {

bool LeaderApp::handleTeleopContinuousValueCommand() {
  uint32_t value = 0U;
  uint16_t requestId = 0U;
  if (!telemetryStreamServer_.consumeTeleopContinuousRequested(value, requestId)) {
    return false;
  }

  beginCommandTracking(requestId, static_cast<uint8_t>(LeaderCommandAction::TeleopContinuousSet));
  handleTeleopContinuousCommand(value, requestId);
  return true;
}

void LeaderApp::handleTeleopContinuousCommand(uint32_t value, uint16_t requestId) {
  (void)requestId;
  const bool enable = (value & 0x1U) != 0U;
  const uint8_t servoIdFilter = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  const uint8_t speedPct = static_cast<uint8_t>((value >> 16U) & 0xFFU);

  if (speedPct <= 100U) {
    teleopContinuousSpeedPct_.store(speedPct);
  }

  if (!enable) {
    releaseFollowerTeleopHold();
  } else {
    teleopContinuousServoIdFilter_.store(servoIdFilter);
    teleopContinuousEnabled_.store(true);
  }

  setLeaderCommandStatus(CommandAckStatus::Applied);
  setFollowerCommandStatus(CommandAckStatus::None);
  if (enable) {
    if (teleopContinuousServoIdFilter_.load() == 0U) {
      setTransientStatus("teleop continuous all", config::leader::kMoveStatusHoldMs);
    } else {
      setTransientStatus("teleop continuous one", config::leader::kMoveStatusHoldMs);
    }
  } else {
    setTransientStatus("teleop continuous off", config::leader::kMoveStatusHoldMs);
  }
}

} // namespace soarm
