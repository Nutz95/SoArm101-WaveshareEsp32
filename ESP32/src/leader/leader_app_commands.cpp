#include "leader_app.h"

#include "leader_retry_policy.h"
#include "leader_servo_command_policy.h"
#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"
#include "../common/servo/servo_control_opcode.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

namespace soarm {

void LeaderApp::handlePairingCommands() {
  uint16_t requestId = 0U;
  if (telemetryStreamServer_.consumeResetPairingRequested(requestId)) {
    handleResetPairingCommand(requestId);
    return;
  }

  uint32_t scanTarget = 0U;
  if (telemetryStreamServer_.consumeServoScanRequested(scanTarget, requestId)) {
    handleServoScanCommand(scanTarget, requestId);
  }
}

void LeaderApp::handleResetPairingCommand(uint16_t requestId) {
  beginCommandTracking(requestId, static_cast<uint8_t>(LeaderCommandAction::ResetPairing));
  const bool resetOk = presenceService_->resetPairing();
  setLeaderCommandStatus(resetOk ? CommandAckStatus::Applied : CommandAckStatus::Failed);
  setFollowerCommandStatus(resetOk ? CommandAckStatus::Accepted : CommandAckStatus::Failed);
  setTransientStatus(resetOk ? "pairing reset" : "pair reset failed", config::leader::kResetPairingStatusHoldMs);
}

void LeaderApp::handleServoScanCommand(uint32_t scanTarget, uint16_t requestId) {
  static constexpr uint32_t kScanTargetBoth = 0U;
  static constexpr uint32_t kScanTargetLeader = 1U;
  static constexpr uint32_t kScanTargetFollower = 2U;

  beginCommandTracking(requestId, commandCodeForScanTarget(scanTarget));

  const bool runLeader = (scanTarget == kScanTargetBoth) || (scanTarget == kScanTargetLeader);
  const bool runFollower = (scanTarget == kScanTargetBoth) || (scanTarget == kScanTargetFollower);

  uint8_t localCount = 0U;
  bool followerSent = true;

  if (runLeader) {
    localCount = servoBusService_.scan();
    setLeaderCommandStatus(CommandAckStatus::Applied);
  } else {
    setLeaderCommandStatus(CommandAckStatus::None);
  }

  if (runFollower) {
    followerSent = presenceService_->requestServoScan(requestId);
    if (followerSent) {
      setFollowerCommandStatus(CommandAckStatus::Accepted);
      setFollowerRetryPayload(static_cast<uint8_t>(ServoControlOpcode::Scan), 0U, config::leader::kFollowerCommandMaxRetries);
      awaitFollowerAck(requestId, static_cast<uint8_t>(ServoControlOpcode::Scan), config::leader::kFollowerScanAckTimeoutMs);
    } else {
      setFollowerCommandStatus(CommandAckStatus::Failed);
    }
  } else {
    setFollowerCommandStatus(CommandAckStatus::None);
  }

  buildScanStatusLine(runLeader, runFollower, followerSent, localCount);
  setTransientStatus(statusLine_, config::leader::kScanStatusHoldMs);
}

uint8_t LeaderApp::commandCodeForScanTarget(uint32_t scanTarget) const {
  switch (scanTarget) {
  case 1U:
    return static_cast<uint8_t>(LeaderCommandAction::ServoScanLeader);
  case 2U:
    return static_cast<uint8_t>(LeaderCommandAction::ServoScanFollower);
  default:
    return static_cast<uint8_t>(LeaderCommandAction::ServoScan);
  }
}

void LeaderApp::buildScanStatusLine(bool runLeader, bool runFollower, bool followerSent, uint8_t localCount) {
  if (runLeader && runFollower) {
    snprintf(statusLine_, sizeof(statusLine_), followerSent ? "scan L:%u + F sent" : "scan L:%u F failed", localCount);
    return;
  }

  if (runLeader) {
    snprintf(statusLine_, sizeof(statusLine_), "scan leader: %u", localCount);
    return;
  }

  strncpy(statusLine_, followerSent ? "scan follower sent" : "scan follower failed", sizeof(statusLine_) - 1);
  statusLine_[sizeof(statusLine_) - 1] = '\0';
}

void LeaderApp::handleServoCommands() {
  if (handleFollowerDebugCommands()) {
    return;
  }

  if (handleLeaderDebugCommands()) {
    return;
  }

  if (handleServoValueCommands()) {
    return;
  }
}

bool LeaderApp::handleFollowerDebugCommands() {
  uint16_t requestId = 0U;
  if (telemetryStreamServer_.consumeServoDebugEnableFollowerRequested(requestId)) {
    return handleFollowerDebugCommand(
        requestId,
        ServoControlOpcode::DebugEnable,
        LeaderCommandAction::ServoDebugEnableFollower,
        "follower debug enable");
  }

  if (telemetryStreamServer_.consumeServoDebugDisableFollowerRequested(requestId)) {
    return handleFollowerDebugCommand(
        requestId,
        ServoControlOpcode::DebugDisable,
        LeaderCommandAction::ServoDebugDisableFollower,
        "follower debug disable");
  }

  return false;
}

bool LeaderApp::handleLeaderDebugCommands() {
  uint16_t requestId = 0U;
  if (telemetryStreamServer_.consumeServoDebugEnableRequested(requestId)) {
    return handleLeaderDebugCommand(
        requestId,
        true,
        LeaderCommandAction::ServoDebugEnable,
        "leader debug enable");
  }

  if (telemetryStreamServer_.consumeServoDebugDisableRequested(requestId)) {
    return handleLeaderDebugCommand(
        requestId,
        false,
        LeaderCommandAction::ServoDebugDisable,
        "leader debug disable");
  }

  return false;
}

bool LeaderApp::handleServoValueCommands() {
  return handleServoMoveValueCommand() ||
         handleServoSetIdValueCommand() ||
         handleServoSetModeValueCommand() ||
         handleTeleopMirrorValueCommand() ||
         handleTeleopContinuousValueCommand();
}

bool LeaderApp::handleFollowerDebugCommand(
    uint16_t requestId,
    ServoControlOpcode op,
    LeaderCommandAction action,
    const char *statusText) {
  beginCommandTracking(requestId, static_cast<uint8_t>(action));
  const bool sent = presenceService_->requestServoControl(static_cast<uint8_t>(op), 0U, requestId);
  setLeaderCommandStatus(CommandAckStatus::None);
  if (sent) {
    setFollowerCommandStatus(CommandAckStatus::Accepted);
    setFollowerRetryPayload(static_cast<uint8_t>(op), 0U, config::leader::kFollowerCommandMaxRetries);
    awaitFollowerAck(requestId, static_cast<uint8_t>(op), config::leader::kFollowerDebugAckTimeoutMs);
  } else {
    setFollowerCommandStatus(CommandAckStatus::Failed);
  }
  setTransientStatus(statusText, config::leader::kDebugStatusHoldMs);
  return true;
}

bool LeaderApp::handleLeaderDebugCommand(
    uint16_t requestId,
    bool enable,
    LeaderCommandAction action,
    const char *statusText) {
  beginCommandTracking(requestId, static_cast<uint8_t>(action));
  servoDebugManual_ = enable;
  servoBusService_.setDebugManual(enable);
  setLeaderCommandStatus(CommandAckStatus::Applied);
  setFollowerCommandStatus(CommandAckStatus::None);
  setTransientStatus(statusText, config::leader::kDebugStatusHoldMs);
  return true;
}

bool LeaderApp::handleServoMoveValueCommand() {
  uint32_t value = 0U;
  uint16_t requestId = 0U;
  if (!telemetryStreamServer_.consumeServoMoveRequested(value, requestId)) {
    return false;
  }

  beginCommandTracking(requestId, static_cast<uint8_t>(LeaderCommandAction::ServoMove));
  handleServoMoveCommand(value, requestId);
  return true;
}

bool LeaderApp::handleServoSetIdValueCommand() {
  uint32_t value = 0U;
  uint16_t requestId = 0U;
  if (!telemetryStreamServer_.consumeServoSetIdRequested(value, requestId)) {
    return false;
  }

  beginCommandTracking(requestId, static_cast<uint8_t>(LeaderCommandAction::ServoSetId));
  handleServoSetIdCommand(value, requestId);
  return true;
}

bool LeaderApp::handleServoSetModeValueCommand() {
  uint32_t value = 0U;
  uint16_t requestId = 0U;
  if (!telemetryStreamServer_.consumeServoSetModeRequested(value, requestId)) {
    return false;
  }

  beginCommandTracking(requestId, static_cast<uint8_t>(LeaderCommandAction::ServoSetMode));
  handleServoSetModeCommand(value, requestId);
  return true;
}

bool LeaderApp::handleTeleopMirrorValueCommand() {
  uint32_t value = 0U;
  uint16_t requestId = 0U;
  if (!telemetryStreamServer_.consumeTeleopMirrorRequested(value, requestId)) {
    return false;
  }

  beginCommandTracking(requestId, static_cast<uint8_t>(LeaderCommandAction::TeleopMirror));
  handleTeleopMirrorCommand(value, requestId);
  return true;
}

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

void LeaderApp::handleServoMoveCommand(uint32_t value, uint16_t requestId) {
  const uint8_t id = static_cast<uint8_t>(value & 0xFFU);
  const int16_t position = static_cast<int16_t>((value >> 8U) & 0xFFFFU);
  const uint8_t speedPct = static_cast<uint8_t>((value >> 24U) & 0xFFU);
  const uint16_t speed = static_cast<uint16_t>(
      (static_cast<uint32_t>(speedPct) * config::leader::kTeleopServoMaxSpeedRaw) / 100U);
  bool ok = false;
  if (servoDebugManual_) {
    ok = servoBusService_.moveTo(id, position, speed, 0U);
    const bool followerSent = presenceService_->requestServoControl(
        static_cast<uint8_t>(ServoControlOpcode::Move),
        value,
        requestId);
    if (followerSent) {
      setFollowerCommandStatus(CommandAckStatus::Accepted);
      setFollowerRetryPayload(static_cast<uint8_t>(ServoControlOpcode::Move), value, config::leader::kFollowerCommandMaxRetries);
      awaitFollowerAck(requestId, static_cast<uint8_t>(ServoControlOpcode::Move), config::leader::kFollowerMoveAckTimeoutMs);
    } else {
      setFollowerCommandStatus(CommandAckStatus::Failed);
    }
  } else {
    setFollowerCommandStatus(CommandAckStatus::Rejected);
  }
  setLeaderCommandStatus(ok ? CommandAckStatus::Applied : CommandAckStatus::Rejected);
  setTransientStatus(ok ? "servo move sent" : "servo move blocked", config::leader::kMoveStatusHoldMs);
}

void LeaderApp::handleServoSetIdCommand(uint32_t value, uint16_t requestId) {
  const uint8_t oldId = static_cast<uint8_t>(value & 0xFFU);
  const uint8_t newId = static_cast<uint8_t>((value >> 8U) & 0xFFU);

  const ServoSetIdRoutingDecision route = decideServoSetIdRouting(
      servoDebugManual_,
      presenceService_->followerServoDebugManual());

  bool ok = false;
  if (route.executeLeaderLocal) {
    ok = servoBusService_.setServoId(oldId, newId);
    if (ok) {
      servoBusService_.scan();
    }
  }

  bool followerSent = false;
  if (route.forwardFollower) {
    followerSent = presenceService_->requestServoControl(
        static_cast<uint8_t>(ServoControlOpcode::SetId),
        value,
        requestId);
    if (followerSent) {
      setFollowerCommandStatus(CommandAckStatus::Accepted);
      setFollowerRetryPayload(static_cast<uint8_t>(ServoControlOpcode::SetId), value, config::leader::kFollowerCommandMaxRetries);
      awaitFollowerAck(requestId, static_cast<uint8_t>(ServoControlOpcode::SetId), config::leader::kFollowerSetIdAckTimeoutMs);
    } else {
      setFollowerCommandStatus(CommandAckStatus::Failed);
    }
  } else {
    setFollowerCommandStatus(CommandAckStatus::Rejected);
  }

  if (route.executeLeaderLocal) {
    setLeaderCommandStatus(ok ? CommandAckStatus::Applied : CommandAckStatus::Rejected);
  } else {
    setLeaderCommandStatus(CommandAckStatus::None);
  }
  if (ok) {
    setTransientStatus("servo id updated", config::leader::kSetIdStatusHoldMs);
  } else if (followerSent) {
    setTransientStatus("follower servo id sent", config::leader::kSetIdStatusHoldMs);
  } else {
    setTransientStatus("servo id blocked", config::leader::kSetIdStatusHoldMs);
  }
}

void LeaderApp::handleServoSetModeCommand(uint32_t value, uint16_t requestId) {
  const uint8_t id = static_cast<uint8_t>(value & 0xFFU);
  const uint8_t mode = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  bool ok = false;
  if (servoDebugManual_) {
    ok = servoBusService_.setServoMode(id, mode);
    const bool followerSent = presenceService_->requestServoControl(
        static_cast<uint8_t>(ServoControlOpcode::SetMode),
        value,
        requestId);
    if (followerSent) {
      setFollowerCommandStatus(CommandAckStatus::Accepted);
      setFollowerRetryPayload(static_cast<uint8_t>(ServoControlOpcode::SetMode), value, config::leader::kFollowerCommandMaxRetries);
      awaitFollowerAck(requestId, static_cast<uint8_t>(ServoControlOpcode::SetMode), config::leader::kFollowerSetModeAckTimeoutMs);
    } else {
      setFollowerCommandStatus(CommandAckStatus::Failed);
    }
  } else {
    setFollowerCommandStatus(CommandAckStatus::Rejected);
  }
  setLeaderCommandStatus(ok ? CommandAckStatus::Applied : CommandAckStatus::Rejected);
  setTransientStatus(ok ? "servo mode updated" : "servo mode blocked", config::leader::kSetModeStatusHoldMs);
}

void LeaderApp::handleTeleopMirrorCommand(uint32_t value, uint16_t requestId) {
  if (!presenceService_->isFollowerLinked()) {
    setLeaderCommandStatus(CommandAckStatus::None);
    setFollowerCommandStatus(CommandAckStatus::Rejected);
    setTransientStatus("teleop follower offline", config::leader::kMoveStatusHoldMs);
    return;
  }

  const bool sent = presenceService_->requestServoControl(
      static_cast<uint8_t>(ServoControlOpcode::TeleopMirror),
      value,
      requestId);
  setLeaderCommandStatus(CommandAckStatus::None);
  if (sent) {
    setFollowerCommandStatus(CommandAckStatus::Accepted);
    setTransientStatus("teleop mirror sent", config::leader::kMoveStatusHoldMs);
  } else {
    setFollowerCommandStatus(CommandAckStatus::Failed);
    setTransientStatus("teleop mirror failed", config::leader::kMoveStatusHoldMs);
  }
}

void LeaderApp::handleTeleopContinuousCommand(uint32_t value, uint16_t requestId) {
  (void)requestId;
  const bool enable = (value & 0x1U) != 0U;
  const uint8_t servoIdFilter = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  if (!enable) {
    teleopContinuousServoIdFilter_.store(0U);
    teleopContinuousEnabled_.store(false);
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