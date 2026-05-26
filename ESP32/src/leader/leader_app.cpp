#include "leader_app.h"
#include "leader_presence_service.h"
#include "leader_retry_policy.h"
#include "leader_servo_telemetry_task.h"
#include "leader_servo_command_policy.h"
#include "leader_teleop_mirror_task.h"
#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"
#include "../common/teleop/teleop_wifi_packet.h"
#include "../common/servo/servo_control_opcode.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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
  teleopContinuousSpeedPct_.store(config::leader::kTeleopContinuousSpeedPct);

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

  if (!teleopWifiBridge_.begin(static_cast<uint16_t>(teleop_wifi::kFollowerListenPort + 1U))) {
    Serial.println("[WARN] Teleop Wi-Fi UDP bridge init failed on leader");
  }

  startBackgroundTasks();
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
  runtimeModeForTasks_.store(static_cast<uint8_t>(mode_));
  updateFollowerState();
  renderStatusLeds();

  LeaderTelemetrySnapshot snapshot{};
  buildTelemetrySnapshot(snapshot, uptimeMs);
  telemetryState_.update(snapshot);

  refreshOled(uptimeMs);

  delay(config::leader::kTickDelayMs);
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

void LeaderApp::updateLocalInputs(uint32_t uptimeMs) {
  localInputs_.joystickPaired  = uptimeMs > config::leader::kJoystickPairReadyMs;
  localInputs_.calibrationDone = !config::leader::kCalibrationRequired ||
                                 uptimeMs > config::leader::kCalibrationReadyMs;
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
  servoBusService_.pollTemperatureAlarmSlow();

  snapshot.uptimeMs = uptimeMs;
  cpuLoadService_.sample(snapshot.cpu0LoadPct, snapshot.cpu1LoadPct);
  snapshot.followerAckRetriesUsed = followerAckRetriesUsed_;
  snapshot.followerAckRttMs = followerAckLastRttMs_;
  snapshot.followerAckTimeoutCount = followerAckTimeoutCount_;
  snapshot.followerAckPending = followerAckPending_ ? 1U : 0U;
  snapshot.teleopMirrorLatencyLastMs = teleopMirrorLatencyMetrics_.lastMs.load();
  snapshot.teleopMirrorLatencyEwmaMs = teleopMirrorLatencyMetrics_.ewmaMs.load();
  snapshot.teleopMirrorLatencyP95Ms = teleopMirrorLatencyMetrics_.p95Ms.load();
  snapshot.teleopMirrorPendingCount = teleopMirrorLatencyMetrics_.pendingCount.load();
  snapshot.teleopMirrorTimeoutCount = teleopMirrorLatencyMetrics_.timeoutCount.load();
  snapshot.teleopContinuousEnabled = teleopContinuousEnabled_.load() ? 1U : 0U;
  snapshot.teleopContinuousServoId = teleopContinuousServoIdFilter_.load();
  snapshot.teleopTransportMode = teleopTransportMode_.load();
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
  snapshot.leaderServoTemperatureAlarm = servoBusService_.hasTemperatureAlarm();
  snapshot.followerServoTemperatureAlarm = presenceService_->followerServoTemperatureAlarm();
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
      char modeLine[24] = "Mode: IDLE";
      buildOledModeLine(modeLine, sizeof(modeLine));

      oled_.showDashboard(
          wifiOta_.ipAddress(),
          followerIpHint_,
          mode_,
          statusLine_,
          modeLine,
          uptimeMs);
    }
  }
}

} // namespace soarm
