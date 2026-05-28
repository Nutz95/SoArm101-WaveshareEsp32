#include "leader_app.h"

#include "../Config/leader_runtime_config.h"

namespace soarm {

bool LeaderApp::handleTeleopTransportValueCommand() {
  uint32_t value = 0U;
  uint16_t requestId = 0U;
  if (!telemetryStreamServer_.consumeTeleopTransportRequested(value, requestId)) {
    return false;
  }

  beginCommandTracking(requestId, static_cast<uint8_t>(LeaderCommandAction::TeleopTransportSet));
  handleTeleopTransportCommand(value, requestId);
  return true;
}

void LeaderApp::handleTeleopTransportCommand(uint32_t value, uint16_t requestId) {
  (void)requestId;

  static constexpr uint32_t kTransportCalibrationLeaderProfile = 2U;
  static constexpr uint32_t kTransportCalibrationFollowerProfile = 3U;

  if (value == kTransportCalibrationLeaderProfile) {
    applyControllerOperationProfile(0U);
    setLeaderCommandStatus(CommandAckStatus::Applied);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("profile cal leader", config::leader::kMoveStatusHoldMs);
    return;
  }

  if (value == kTransportCalibrationFollowerProfile) {
    applyControllerOperationProfile(1U);
    setLeaderCommandStatus(CommandAckStatus::Applied);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("profile cal follower", config::leader::kMoveStatusHoldMs);
    return;
  }

  if (value > static_cast<uint32_t>(TeleopTransportMode::WifiUdp)) {
    setLeaderCommandStatus(CommandAckStatus::Rejected);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("teleop transport invalid", config::leader::kMoveStatusHoldMs);
    return;
  }

  applyControllerOperationProfile(value == static_cast<uint32_t>(TeleopTransportMode::WifiUdp) ? 3U : 2U);
  teleopTransportMode_.store(static_cast<uint8_t>(value));
  setLeaderCommandStatus(CommandAckStatus::Applied);
  setFollowerCommandStatus(CommandAckStatus::None);

  if (static_cast<TeleopTransportMode>(value) == TeleopTransportMode::WifiUdp) {
    setTransientStatus("teleop transport wifi", config::leader::kMoveStatusHoldMs);
  } else {
    setTransientStatus("teleop transport espnow", config::leader::kMoveStatusHoldMs);
  }
}

} // namespace soarm
