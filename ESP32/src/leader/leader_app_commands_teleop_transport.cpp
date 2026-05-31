#include "leader_app.h"

#include "../Config/leader_runtime_config.h"
#include "../common/controller/controller_operation_profile.h"
#include "../common/teleop/teleop_transport_set_values.h"
#include "../common/teleop/teleop_transport_mode.h"

namespace soarm {

using teleop_transport_set::kCalibrationFollower;
using teleop_transport_set::kCalibrationLeader;
using teleop_transport_set::kEspNow;
using teleop_transport_set::kPassthrough;
using teleop_transport_set::kPcSerial;
using teleop_transport_set::kWifiUdp;

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

  if (passthroughEngaged_.load()) {
    disengagePassthroughMode(ControllerOperationProfile::TeleopEspNow);
  }

  if (value == kCalibrationLeader) {
    applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::CalibrationLeader));
    setLeaderCommandStatus(CommandAckStatus::Applied);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("cal leader place near ctr", config::leader::kMoveStatusHoldMs);
    return;
  }

  if (value == kCalibrationFollower) {
    applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::CalibrationFollower));
    setLeaderCommandStatus(CommandAckStatus::Applied);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("cal follower? press A", config::leader::kMoveStatusHoldMs);
    return;
  }

  if (value == kPassthrough) {
    applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::Passthrough));
    engagePassthroughMode();
    setLeaderCommandStatus(CommandAckStatus::Applied);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("passthrough active", config::leader::kMoveStatusHoldMs);
    return;
  }

  if (value == kPcSerial) {
    applyControllerOperationProfile(toProfileRaw(ControllerOperationProfile::TeleopPcSerial));
    setLeaderCommandStatus(CommandAckStatus::Applied);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("pc serial: start COM bridge", config::leader::kMoveStatusHoldMs);
    return;
  }

  if (value > kWifiUdp) {
    setLeaderCommandStatus(CommandAckStatus::Rejected);
    setFollowerCommandStatus(CommandAckStatus::None);
    setTransientStatus("teleop transport invalid", config::leader::kMoveStatusHoldMs);
    return;
  }

  applyControllerOperationProfile(
      value == kWifiUdp ? toProfileRaw(ControllerOperationProfile::TeleopWifi)
                        : toProfileRaw(ControllerOperationProfile::TeleopEspNow));
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
