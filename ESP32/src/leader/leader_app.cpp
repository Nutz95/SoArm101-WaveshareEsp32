#include "leader_app.h"
#include "leader_presence_service.h"
#include "leader_servo_command_policy.h"
#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"
#include "../common/servo/servo_control_opcode.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

// WiFi credentials injected from environment variables at build time.
// See platformio.ini build_flags for WIFI_SSID / WIFI_PASS definitions.
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

#ifndef FOLLOWER_OTA_IP_HINT
#define FOLLOWER_OTA_IP_HINT ""
#endif

#ifndef LEADER_SERVO_BUS_RX_PIN
#define LEADER_SERVO_BUS_RX_PIN 18
#endif
#ifndef LEADER_SERVO_BUS_TX_PIN
#define LEADER_SERVO_BUS_TX_PIN 19
#endif
#ifndef LEADER_SERVO_BUS_BAUD
#define LEADER_SERVO_BUS_BAUD 1000000U
#endif

namespace soarm {

LeaderApp::LeaderApp()
    : statusLedService_(STATUS_LED_PIN, STATUS_LED_COUNT),
      wifiOta_(WIFI_SSID, WIFI_PASS, "soarm-leader"),
  presenceService_(std::unique_ptr<ILeaderPresenceService>(new LeaderPresenceService())),
      oledConfig_{128, 32, 0x3C, 300, false, OledTextStyle::Small},
      oled_(oledConfig_),
  telemetryState_(),
  telemetryStreamServer_(telemetryState_),
      localInputs_{true, false, false, false},
      followerState_(ArmRuntimeState::WaitingEspNow),
      mode_(OperationMode::Idle),
      followerIpHint_{0},
      statusLine_{0},
      lastOledRefreshMs_(0U) {
  strncpy(followerIpHint_, FOLLOWER_OTA_IP_HINT, sizeof(followerIpHint_) - 1);
  followerIpHint_[sizeof(followerIpHint_) - 1] = '\0';

  strncpy(statusLine_, "boot", sizeof(statusLine_) - 1);
  statusLine_[sizeof(statusLine_) - 1] = '\0';
}

void LeaderApp::begin() {
  Serial.begin(115200);

  statusLedService_.begin();

  if (!oled_.begin()) {
    Serial.println("[WARN] OLED not found");
  }
  oled_.showConnecting(followerIpHint_);

  const bool nvsReady = calibrationStore_.begin();
  CalibrationProfile leaderProfile{};
  if (!nvsReady || !calibrationStore_.load(ArmRole::Leader, leaderProfile)) {
    const CalibrationProfile defaults = calibrationStore_.buildDefaultProfile();
    calibrationStore_.save(ArmRole::Leader, defaults);
  }

  WifiOtaCallbacks cb;
  cb.onWifiConnected    = [](const char *ip) { Serial.printf("[WiFi] connected ip=%s\n", ip); };
  cb.onWifiDisconnected = []() { Serial.println("[WiFi] disconnected"); };
  cb.onOtaBegin         = []() { Serial.println("[OTA] begin"); };
  cb.onOtaEnd           = []() { Serial.println("[OTA] end"); };
  cb.onOtaError         = [](uint32_t code) { Serial.printf("[OTA] error %u\n", code); };
  wifiOta_.begin(cb);

  const bool espNowReady = presenceService_->begin();
  if (!espNowReady) {
    Serial.println("[WARN] ESP-NOW init failed on leader");
  }

  ServoBusConfig servoBusConfig{};
  servoBusConfig.serial = &Serial2;
  servoBusConfig.rxPin = LEADER_SERVO_BUS_RX_PIN;
  servoBusConfig.txPin = LEADER_SERVO_BUS_TX_PIN;
  servoBusConfig.baudRate = LEADER_SERVO_BUS_BAUD;
  servoBusConfig.firstId = 1U;
  servoBusConfig.lastId = 32U;
  if (!servoBusService_.begin(servoBusConfig)) {
    Serial.println("[WARN] Servo bus init failed on leader");
  }

  const uint8_t localScanCount = servoBusService_.scan();
  leaderStartupScanDone_ = true;
    leaderServoFault_ = localScanCount != config::common::kExpectedLeaderServoCount;
  if (leaderServoFault_) {
    Serial.printf(
        "[SERVO] leader startup mismatch expected=%u actual=%u\n",
      static_cast<unsigned>(config::common::kExpectedLeaderServoCount),
        static_cast<unsigned>(localScanCount));
  }

  if (!telemetryStreamServer_.begin(9090)) {
    Serial.println("[WARN] Telemetry stream init failed");
  } else {
    Serial.println("[INFO] Telemetry stream on :9090");
  }
}

void LeaderApp::tick() {
  wifiOta_.tick();
  presenceService_->tick();
  telemetryStreamServer_.tick();

  handlePairingCommands();
  handleServoCommands();

  const uint32_t uptimeMs = millis();
  runStartupServoScans(uptimeMs);
  updateFollowerAckTracking(uptimeMs);
  updateLocalInputs(uptimeMs);
  updateServoHealthFlags();
  computeModeAndStatus();
  updateFollowerState();
  renderStatusLeds();

  LeaderTelemetrySnapshot snapshot{};
  buildTelemetrySnapshot(snapshot, uptimeMs);
  telemetryState_.update(snapshot);

  refreshOled(uptimeMs);

  delay(config::leader::kTickDelayMs);
}

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
  return handleServoMoveValueCommand() || handleServoSetIdValueCommand() || handleServoSetModeValueCommand();
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
      // Refresh local servo inventory immediately after ID change.
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

  setLeaderCommandStatus(ok ? CommandAckStatus::Applied : CommandAckStatus::Rejected);
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

void LeaderApp::beginCommandTracking(uint16_t requestId, uint8_t commandCode) {
  commandRequestId_ = requestId;
  commandCode_ = commandCode;
  leaderCommandStatus_ = CommandAckStatus::Accepted;
  followerCommandStatus_ = CommandAckStatus::Accepted;
  followerAckPending_ = false;
  followerRetryEnabled_ = false;
  followerRetryRemaining_ = 0U;
}

void LeaderApp::setLeaderCommandStatus(CommandAckStatus status) {
  leaderCommandStatus_ = status;
}

void LeaderApp::setFollowerCommandStatus(CommandAckStatus status) {
  followerCommandStatus_ = status;
}

void LeaderApp::awaitFollowerAck(uint16_t requestId, uint8_t op, uint32_t timeoutMs) {
  followerAckPending_ = true;
  followerAckRequestId_ = requestId;
  followerAckCommandOp_ = op;
  followerAckDeadlineMs_ = millis() + timeoutMs;
  followerNextRetryMs_ = millis() + config::leader::kFollowerRetryIntervalMs;
}

void LeaderApp::setFollowerRetryPayload(uint8_t op, uint32_t value, uint8_t maxRetries) {
  followerRetryEnabled_ = true;
  followerRetryOp_ = op;
  followerRetryValue_ = value;
  followerRetryRemaining_ = maxRetries;
}

void LeaderApp::updateFollowerAckTracking(uint32_t nowMs) {
  if (!followerAckPending_) {
    return;
  }

  const bool requestIdMatch = presenceService_->followerLastAckRequestId() == followerAckRequestId_;
  const bool opMatch = presenceService_->followerLastAckCommandOp() == followerAckCommandOp_;
  if (requestIdMatch && opMatch) {
    followerCommandStatus_ = static_cast<CommandAckStatus>(presenceService_->followerLastAckStatus());
    followerAckPending_ = false;
    followerRetryEnabled_ = false;
    return;
  }

  if (followerRetryEnabled_ && followerRetryRemaining_ > 0U && nowMs >= followerNextRetryMs_) {
    const bool resent = presenceService_->requestServoControl(
        followerRetryOp_,
        followerRetryValue_,
        followerAckRequestId_);
    followerRetryRemaining_ = static_cast<uint8_t>(followerRetryRemaining_ - 1U);
    followerNextRetryMs_ = nowMs + config::leader::kFollowerRetryIntervalMs;
    if (!resent && followerRetryRemaining_ == 0U) {
      followerCommandStatus_ = CommandAckStatus::Failed;
      followerAckPending_ = false;
      followerRetryEnabled_ = false;
      return;
    }
  }

  if (nowMs >= followerAckDeadlineMs_) {
    followerCommandStatus_ = CommandAckStatus::Timeout;
    followerAckPending_ = false;
    followerRetryEnabled_ = false;
  }
}

void LeaderApp::runStartupServoScans(uint32_t nowMs) {
  if (!leaderStartupScanDone_) {
    const uint8_t localScanCount = servoBusService_.scan();
    leaderStartupScanDone_ = true;
    leaderServoFault_ = localScanCount != config::common::kExpectedLeaderServoCount;
  }

  if (!presenceService_->isFollowerLinked() || followerStartupScanDone_) {
    return;
  }

  if (followerStartupScanPending_) {
    const bool requestIdMatch = presenceService_->followerLastAckRequestId() == followerStartupScanRequestId_;
    const bool opMatch = presenceService_->followerLastAckCommandOp() == static_cast<uint8_t>(ServoControlOpcode::Scan);
    const bool applied = presenceService_->followerLastAckStatus() == static_cast<uint8_t>(CommandAckStatus::Applied);
    if (requestIdMatch && opMatch && applied) {
      followerStartupScanDone_ = true;
      followerStartupScanPending_ = false;
      return;
    }

    if (nowMs < followerStartupScanDeadlineMs_) {
      return;
    }

    followerStartupScanPending_ = false;
  }

  if (nowMs < followerStartupScanRetryMs_) {
    return;
  }

  followerStartupScanRequestId_ = static_cast<uint16_t>(followerStartupScanRequestId_ + 1U);
  const bool sent = presenceService_->requestServoScan(followerStartupScanRequestId_);
  followerStartupScanRetryMs_ = nowMs + config::leader::kFollowerScanRetryIntervalMs;
  if (!sent) {
    return;
  }
  followerStartupScanPending_ = true;
  followerStartupScanDeadlineMs_ = nowMs + config::leader::kFollowerScanRetryIntervalMs;
}

void LeaderApp::updateServoHealthFlags() {
  leaderServoFault_ = servoBusService_.lastScanCount() != config::common::kExpectedLeaderServoCount;

  if (!presenceService_->isFollowerLinked()) {
    followerServoFault_ = false;
    return;
  }

  followerServoFault_ = presenceService_->followerServoCount() != config::common::kExpectedFollowerServoCount;
}

void LeaderApp::setTransientStatus(const char *text, uint32_t holdMs) {
  strncpy(statusLine_, text, sizeof(statusLine_) - 1);
  statusLine_[sizeof(statusLine_) - 1] = '\0';
  commandStatusHoldUntilMs_ = millis() + holdMs;
}

void LeaderApp::updateLocalInputs(uint32_t uptimeMs) {
  localInputs_.joystickPaired  = uptimeMs > config::leader::kJoystickPairReadyMs;
  localInputs_.calibrationDone = uptimeMs > config::leader::kCalibrationReadyMs;
  localInputs_.espNowLinked    = presenceService_->isFollowerLinked();
}

void LeaderApp::computeModeAndStatus() {
  const bool followerIpValid = presenceService_->hasValidFollowerIp();

  if (millis() < commandStatusHoldUntilMs_) {
    mode_ = localInputs_.espNowLinked ? OperationMode::Teleoperation : OperationMode::Idle;
    return;
  }

  if (leaderServoFault_ || followerServoFault_) {
    mode_ = localInputs_.espNowLinked ? OperationMode::Teleoperation : OperationMode::Idle;
    snprintf(
        statusLine_,
        sizeof(statusLine_),
        "servo mismatch L:%u F:%u",
        static_cast<unsigned>(servoBusService_.lastScanCount()),
        static_cast<unsigned>(presenceService_->followerServoCount()));
    statusLine_[sizeof(statusLine_) - 1] = '\0';
    return;
  }

  if (!localInputs_.joystickPaired) {
    mode_ = OperationMode::Idle;
    strncpy(statusLine_, "pair joystick", sizeof(statusLine_) - 1);
  } else if (!localInputs_.calibrationDone) {
    mode_ = OperationMode::CalibrationLeader;
    strncpy(statusLine_, "calibration", sizeof(statusLine_) - 1);
  } else if (!localInputs_.espNowLinked) {
    mode_ = OperationMode::Idle;
    if (presenceService_->followerIp()[0] != '\0' && !followerIpValid) {
      strncpy(statusLine_, "follower wifi down", sizeof(statusLine_) - 1);
    } else {
      strncpy(statusLine_, "follower offline", sizeof(statusLine_) - 1);
    }
  } else {
    mode_ = OperationMode::Teleoperation;
    strncpy(statusLine_, "teleop ready", sizeof(statusLine_) - 1);
  }
  statusLine_[sizeof(statusLine_) - 1] = '\0';
}

void LeaderApp::updateFollowerState() {
  if (localInputs_.espNowLinked) {
    followerState_ = followerServoFault_ ? ArmRuntimeState::ServoFault : ArmRuntimeState::Ready;
    strncpy(followerIpHint_, presenceService_->followerIp(), sizeof(followerIpHint_) - 1);
    followerIpHint_[sizeof(followerIpHint_) - 1] = '\0';
  } else {
    followerState_ = ArmRuntimeState::WaitingEspNow;
    if (!presenceService_->hasValidFollowerIp()) {
      strncpy(followerIpHint_, "0.0.0.0", sizeof(followerIpHint_) - 1);
      followerIpHint_[sizeof(followerIpHint_) - 1] = '\0';
    }
  }
}

void LeaderApp::renderStatusLeds() {
  ArmRuntimeState localState = stateMachine_.computeState(localInputs_);
  if (leaderServoFault_) {
    localState = ArmRuntimeState::ServoFault;
  }

  ArmRuntimeState followerLedState = followerState_;
  if (followerServoFault_) {
    followerLedState = ArmRuntimeState::ServoFault;
  }

  statusLedService_.render(0, localState);
  if (STATUS_LED_COUNT > 1U) {
    statusLedService_.render(1, followerLedState);
  }
}

void LeaderApp::buildTelemetrySnapshot(LeaderTelemetrySnapshot &snapshot, uint32_t uptimeMs) {
  snapshot.uptimeMs = uptimeMs;
  cpuLoadService_.sample(snapshot.cpu0LoadPct, snapshot.cpu1LoadPct);
  snapshot.reserved0 = 0;
  snapshot.reserved1 = 0;
  strncpy(snapshot.leaderIp, wifiOta_.ipAddress(), sizeof(snapshot.leaderIp) - 1);
  snapshot.leaderIp[sizeof(snapshot.leaderIp) - 1] = '\0';
  strncpy(snapshot.followerIp, followerIpHint_, sizeof(snapshot.followerIp) - 1);
  snapshot.followerIp[sizeof(snapshot.followerIp) - 1] = '\0';
  snapshot.leaderState = leaderServoFault_ ? ArmRuntimeState::ServoFault : stateMachine_.computeState(localInputs_);
  snapshot.followerState = followerState_;
  snapshot.mode = mode_;
  strncpy(snapshot.status, statusLine_, sizeof(snapshot.status) - 1);
  snapshot.status[sizeof(snapshot.status) - 1] = '\0';
  snapshot.joystickPaired = localInputs_.joystickPaired;
  snapshot.calibrationDone = localInputs_.calibrationDone;
  snapshot.espNowLinked = localInputs_.espNowLinked;
  snapshot.pairingLocked = presenceService_->isPaired();
  snapshot.leaderServoDebugManual = servoBusService_.isDebugManual();
  snapshot.followerServoDebugManual = presenceService_->followerServoDebugManual();
  snapshot.leaderServoCount = servoBusService_.lastScanCount();
  snapshot.followerServoCount = presenceService_->followerServoCount();
  strncpy(snapshot.leaderMac, presenceService_->localMac(), sizeof(snapshot.leaderMac) - 1);
  snapshot.leaderMac[sizeof(snapshot.leaderMac) - 1] = '\0';
  strncpy(snapshot.followerMac, presenceService_->pairedPeerMac(), sizeof(snapshot.followerMac) - 1);
  snapshot.followerMac[sizeof(snapshot.followerMac) - 1] = '\0';
  strncpy(snapshot.leaderServoIds, servoBusService_.lastIdsText(), sizeof(snapshot.leaderServoIds) - 1);
  snapshot.leaderServoIds[sizeof(snapshot.leaderServoIds) - 1] = '\0';
  strncpy(snapshot.followerServoIds, presenceService_->followerServoIds(), sizeof(snapshot.followerServoIds) - 1);
  snapshot.followerServoIds[sizeof(snapshot.followerServoIds) - 1] = '\0';
  strncpy(snapshot.leaderServoTelemetry, servoBusService_.lastTelemetryText(), sizeof(snapshot.leaderServoTelemetry) - 1);
  snapshot.leaderServoTelemetry[sizeof(snapshot.leaderServoTelemetry) - 1] = '\0';
  strncpy(snapshot.followerServoTelemetry, presenceService_->followerServoTelemetry(), sizeof(snapshot.followerServoTelemetry) - 1);
  snapshot.followerServoTelemetry[sizeof(snapshot.followerServoTelemetry) - 1] = '\0';
  snapshot.commandRequestId = commandRequestId_;
  snapshot.commandCode = commandCode_;
  snapshot.leaderCommandStatus = static_cast<uint8_t>(leaderCommandStatus_);
  snapshot.followerCommandStatus = static_cast<uint8_t>(followerCommandStatus_);
}

void LeaderApp::refreshOled(uint32_t uptimeMs) {
  if ((uptimeMs - lastOledRefreshMs_) >= oledConfig_.refreshPeriodMs) {
    lastOledRefreshMs_ = uptimeMs;
    if (wifiOta_.isOtaInProgress()) {
      strncpy(statusLine_, "ota updating", sizeof(statusLine_) - 1);
      statusLine_[sizeof(statusLine_) - 1] = '\0';
      oled_.showOtaProgress(50);
    } else {
      oled_.showDashboard(
          wifiOta_.ipAddress(),
          followerIpHint_,
          mode_,
          statusLine_,
          presenceService_->isPaired() ? "P:locked" : "P:open",
          uptimeMs);
    }
  }
}

} // namespace soarm
