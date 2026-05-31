#include "leader_app.h"
#include "leader_presence_service.h"
#include "leader_retry_policy.h"
#include "leader_servo_telemetry_task.h"
#include "leader_servo_command_policy.h"
#include "leader_teleop_mirror_task.h"
#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"
#include "../common/teleop/teleop_wifi_packet.h"
#include "../common/teleop/teleop_transport_mode.h"
#include "../common/servo/servo_control_opcode.h"
#include "../common/controller/controller_operation_profile.h"

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
#ifndef USB_CDC_BAUD
#define USB_CDC_BAUD 1000000U
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
  Serial.begin(USB_CDC_BAUD);

  statusLedService_.begin();
  teleopContinuousSpeedPct_.store(config::leader::kTeleopContinuousSpeedPct);

  if (!oled_.begin()) {
    Serial.println("[WARN] OLED not found");
  }
  oled_.showConnecting(followerIpHint_);

  const bool nvsReady = calibrationStore_.begin();
  if (!nvsReady || !calibrationStore_.load(ArmRole::Leader, leaderCalibrationProfile_)) {
    const CalibrationProfile defaults = calibrationStore_.buildDefaultProfile();
    calibrationStore_.save(ArmRole::Leader, defaults);
    leaderCalibrationProfile_ = defaults;
  }
  if (!nvsReady || !calibrationStore_.load(ArmRole::Follower, followerCalibrationProfile_)) {
    const CalibrationProfile defaults = calibrationStore_.buildDefaultProfile();
    calibrationStore_.save(ArmRole::Follower, defaults);
    followerCalibrationProfile_ = defaults;
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

  teleopPcSerialBridge_.attach(Serial);

  xboxControllerService_.begin();

  startBackgroundTasks();
}

void LeaderApp::tick() {
  wifiOta_.tick();
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  const bool passthroughActive = passthroughEngaged_.load();
  presenceService_->setPairingWatchdogSuspended(
      (profile == ControllerOperationProfile::CalibrationLeader ||
       profile == ControllerOperationProfile::CalibrationFollower) ||
      calibrationPhase_.load() != 0U ||
      followerCalibrationCenterPending_.load());
  presenceService_->tick();
  telemetryStreamServer_.tick();

  handlePairingCommands();
  if (!passthroughActive) {
    handleServoCommands();
  } else {
    (void)handleTeleopTransportValueCommand();
  }

  const uint32_t uptimeMs = millis();
  if (!passthroughActive) {
    runStartupServoScans(uptimeMs);
    updateFollowerAckTracking(uptimeMs);
    pollFollowerCalibrationCenterAck(uptimeMs);
  }
  updateLocalInputs(uptimeMs);
  handleControllerModeCycleEvents();
  if (!passthroughActive && calibrationPhase_.load() == 1U) {
    (void)sampleCalibrationRangeCapture();
  }
  if (!passthroughActive) {
    updateServoHealthFlags();
  }
  computeModeAndStatus();
  runtimeModeForTasks_.store(static_cast<uint8_t>(mode_));
  updateFollowerState();
  renderStatusLeds();

  if (!passthroughActive) {
    const bool pcSerialMirrorBench =
        mode_ == OperationMode::Teleoperation &&
        static_cast<TeleopTransportMode>(teleopTransportMode_.load()) == TeleopTransportMode::PcSerialBridge;
    const uint32_t snapshotPeriodMs =
        pcSerialMirrorBench
            ? config::leader::kTelemetrySnapshotPcSerialMirrorPeriodMs
            : (mode_ == OperationMode::Teleoperation
                   ? config::leader::kTelemetrySnapshotTeleopPeriodMs
                   : config::leader::kTelemetrySnapshotIdlePeriodMs);
    if ((uptimeMs - lastTelemetrySnapshotMs_) >= snapshotPeriodMs) {
      lastTelemetrySnapshotMs_ = uptimeMs;
      LeaderTelemetrySnapshot snapshot{};
      buildTelemetrySnapshot(snapshot, uptimeMs);
      telemetryState_.update(snapshot);
    }
  }

  refreshOled(uptimeMs);

  if (passthroughActive) {
    servoPassthrough_.tick(true);
  }

  delay(passthroughActive ? 0U : config::leader::kTickDelayMs);
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
  (void)uptimeMs;
  xboxControllerService_.tick();
  localInputs_.joystickPaired = xboxControllerService_.isControllerPaired();
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  const bool profileCalibration =
      profile == ControllerOperationProfile::CalibrationLeader ||
      profile == ControllerOperationProfile::CalibrationFollower;
  if (profileCalibration) {
    localInputs_.calibrationDone = false;
  } else {
    localInputs_.calibrationDone = !config::leader::kCalibrationRequired ||
                                   uptimeMs > config::leader::kCalibrationReadyMs;
  }
  localInputs_.espNowLinked    = presenceService_->isFollowerLinked();
}

void LeaderApp::buildTelemetrySnapshot(LeaderTelemetrySnapshot &snapshot, uint32_t uptimeMs) {
  servoBusService_.pollTemperatureAlarmSlow();
  XboxRuntimeSnapshot xboxRuntime{};
  xboxControllerService_.snapshot(xboxRuntime);

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
  if (static_cast<TeleopTransportMode>(teleopTransportMode_.load()) == TeleopTransportMode::WifiUdp &&
      !config::leader::kTeleopWifiRequireAck) {
    snapshot.teleopMirrorPendingCount = teleopMirrorLatencyMetrics_.sendFailCount.load();
  }
  snapshot.teleopContinuousEnabled = teleopContinuousEnabled_.load() ? 1U : 0U;
  snapshot.teleopContinuousServoId = teleopContinuousServoIdFilter_.load();
  snapshot.teleopTransportMode = teleopTransportMode_.load();
  snapshot.xboxRuntimeState = xboxRuntime.state;
  snapshot.xboxLastReportAgeMs = xboxRuntime.lastReportAgeMs;
  snapshot.xboxReportCount = xboxRuntime.reportCount;
  snapshot.xboxButtonsMask = xboxRuntime.buttonsMask;
  snapshot.xboxAxisLeftX = xboxRuntime.axisLeftX;
  snapshot.xboxAxisLeftY = xboxRuntime.axisLeftY;
  snapshot.xboxAxisRightX = xboxRuntime.axisRightX;
  snapshot.xboxAxisRightY = xboxRuntime.axisRightY;
  snapshot.xboxDpadX = xboxRuntime.dpadX;
  snapshot.xboxDpadY = xboxRuntime.dpadY;
  snapshot.xboxTriggerLeft = xboxRuntime.triggerLeft;
  snapshot.xboxTriggerRight = xboxRuntime.triggerRight;
  snapshot.xboxLinkEncrypted = xboxRuntime.linkEncrypted;
  snapshot.xboxInputSubscribed = xboxRuntime.inputSubscribed;
  snapshot.xboxControllerPaired = xboxRuntime.controllerPaired;
  strncpy(snapshot.xboxControllerName, xboxRuntime.controllerName, sizeof(snapshot.xboxControllerName) - 1U);
  snapshot.xboxControllerName[sizeof(snapshot.xboxControllerName) - 1U] = '\0';
  strncpy(snapshot.leaderIp, wifiOta_.ipAddress(), sizeof(snapshot.leaderIp) - 1);
  snapshot.leaderIp[sizeof(snapshot.leaderIp) - 1] = '\0';
  if (presenceService_->hasValidFollowerIp()) {
    strncpy(snapshot.followerIp, presenceService_->followerIp(), sizeof(snapshot.followerIp) - 1);
  } else {
    strncpy(snapshot.followerIp, followerIpHint_, sizeof(snapshot.followerIp) - 1);
  }
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
  snapshot.controllerOperationProfile = controllerOperationProfile_.load();
  snapshot.calibrationPhase = calibrationPhase_.load();
  for (uint8_t i = 0U; i < CalibrationProfile::kServoCount; ++i) {
    snapshot.leaderCalibrationMin[i] = leaderCalibrationProfile_.minPosition[i];
    snapshot.leaderCalibrationMax[i] = leaderCalibrationProfile_.maxPosition[i];
    snapshot.followerCalibrationMin[i] = followerCalibrationProfile_.minPosition[i];
    snapshot.followerCalibrationMax[i] = followerCalibrationProfile_.maxPosition[i];
    snapshot.leaderWorkingCalibrationMin[i] = leaderCalibrationWorkingProfile_.minPosition[i];
    snapshot.leaderWorkingCalibrationMax[i] = leaderCalibrationWorkingProfile_.maxPosition[i];
    snapshot.followerWorkingCalibrationMin[i] = followerCalibrationWorkingProfile_.minPosition[i];
    snapshot.followerWorkingCalibrationMax[i] = followerCalibrationWorkingProfile_.maxPosition[i];
  }
  fillLeaderMirrorPositions(snapshot);
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
          static_cast<TeleopTransportMode>(teleopTransportMode_.load()),
          statusLine_,
          uptimeMs);
    }
  }
}

} // namespace soarm
