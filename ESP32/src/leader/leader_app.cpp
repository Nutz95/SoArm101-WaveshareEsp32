#include "leader_app.h"
#include "leader_ble_config.h"
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
#include "../common/usb_debug_log_gate.h"
#include "leader_presence_service.h"

#include <Arduino.h>
#include <esp_system.h>
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
      oledMenu_(oled_),
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
  delay(50);
  bootMs_ = millis();
  USB_DEBUG_LOGF(
      "\n[BOOT] leader reset_reason=%d USB_CDC_BAUD=%lu\n",
      static_cast<int>(esp_reset_reason()),
      static_cast<unsigned long>(USB_CDC_BAUD));

  statusLedService_.begin();
  teleopContinuousSpeedPct_.store(config::leader::kTeleopContinuousSpeedPct);
  USB_DEBUG_LOGLN("[BOOT] stage: status led");

  const bool nvsReady = calibrationStore_.begin();
  USB_DEBUG_LOGLN("[BOOT] stage: nvs");
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

#if LEADER_ENABLE_XBOX_BLE
  USB_DEBUG_LOGLN("[BOOT] stage: xbox BLE");
  xboxControllerService_.begin();
  deferredBleReady_ = true;
#endif

  WifiOtaCallbacks cb;
  cb.onWifiConnected    = [](const char *ip) { USB_DEBUG_LOGF("[WiFi] connected ip=%s\n", ip); };
  cb.onWifiDisconnected = []() { USB_DEBUG_LOGLN("[WiFi] disconnected"); };
  cb.onOtaBegin         = []() { USB_DEBUG_LOGLN("[OTA] begin"); };
  cb.onOtaEnd           = []() { USB_DEBUG_LOGLN("[OTA] end"); };
  cb.onOtaError         = [](uint32_t code) { USB_DEBUG_LOGF("[OTA] error %u\n", code); };
  wifiOta_.begin(cb);
  USB_DEBUG_LOGLN("[BOOT] stage: wifi ota");

  const bool espNowReady = presenceService_->begin();
  USB_DEBUG_LOGF("[BOOT] stage: esp-now ok=%d\n", static_cast<int>(espNowReady));
  if (!espNowReady) {
    USB_DEBUG_LOGLN("[WARN] ESP-NOW init failed on leader");
  }

  USB_DEBUG_LOGLN("[BOOT] leader minimal begin done (heavy init deferred)");
}

void LeaderApp::tick() {
  const uint32_t uptimeMs = millis();
  const bool passthroughActive = passthroughEngaged_.load();

  tickCoreServices(uptimeMs);
  tickCommandHandlers(passthroughActive);
  tickControlState(uptimeMs, passthroughActive);
  tickTelemetrySnapshot(uptimeMs, passthroughActive);
  refreshOled(uptimeMs);

  if (passthroughActive) {
    servoPassthrough_.tick(true);
  }

  delay(passthroughActive ? 0U : config::leader::kTickDelayMs);
}

void LeaderApp::tickCoreServices(uint32_t uptimeMs) {
  runDeferredBootStages(uptimeMs);

  wifiOta_.tick();
  updateEspNowStaPrime(uptimeMs);
  if (wifiDirectLinkEngaged_.load()) {
    auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get());
    if (presence != nullptr) {
      wifiDirectSession_.tick(*presence, wifiDirectRadio_, uptimeMs);
    }
  }
  presenceService_->setPairingWatchdogSuspended(
      calibrationEngaged_.load() || calibrationPhase_.load() != 0U ||
      followerCalibrationCenterPending_.load() || wifiDirectLinkEngaged_.load() ||
      otaEngaged_.load());
  presenceService_->tick();
  clearDeferHomeStaReconnectIfDone();
  telemetryStreamServer_.tick();
  usbDebugService_.tick();
}

void LeaderApp::tickCommandHandlers(bool passthroughActive) {
  handlePairingCommands();
  if (!passthroughActive) {
    handleServoCommands();
  } else {
    (void)handleTeleopTransportValueCommand();
  }
}

void LeaderApp::tickControlState(uint32_t uptimeMs, bool passthroughActive) {
  if (!passthroughActive) {
    runStartupServoScans(uptimeMs);
    updateFollowerAckTracking(uptimeMs);
    pollFollowerCalibrationCenterAck(uptimeMs);
  }
  updateLocalInputs(uptimeMs);
  handleControllerModeCycleEvents();
  if (!passthroughActive && calibrationEngaged_.load() && calibrationPhase_.load() == 1U) {
    (void)sampleCalibrationRangeCapture();
  }
  if (!passthroughActive) {
    updateServoHealthFlags();
  }
  computeModeAndStatus();
  updateTurboOledStatus(uptimeMs);
  runtimeModeForTasks_.store(static_cast<uint8_t>(mode_));
  updateFollowerState();
  renderStatusLeds();
}

void LeaderApp::tickTelemetrySnapshot(uint32_t uptimeMs, bool passthroughActive) {
  if (!passthroughActive) {
    const uint32_t snapshotPeriodMs =
        mode_ == OperationMode::Teleoperation
            ? config::leader::kTelemetrySnapshotTeleopPeriodMs
            : config::leader::kTelemetrySnapshotIdlePeriodMs;
    if ((uptimeMs - lastTelemetrySnapshotMs_) >= snapshotPeriodMs) {
      lastTelemetrySnapshotMs_ = uptimeMs;
      LeaderTelemetrySnapshot snapshot{};
      buildTelemetrySnapshot(snapshot, uptimeMs);
      telemetryState_.update(snapshot);
    }
  }
}

void LeaderApp::runStartupServoScans(uint32_t nowMs) {
  if (!leaderStartupScanDone_) {
    const uint8_t localScanCount = servoBusService_.scan();
    leaderStartupScanDone_ = true;
    leaderServoFault_ = localScanCount != config::common::kExpectedLeaderServoCount;
  }

  if (teleopContinuousEnabled_.load() || deferHomeStaReconnect_.load() || followerAckPending_) {
    return;
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

void LeaderApp::updateLocalInputs(uint32_t uptimeMs) {
  (void)uptimeMs;
  xboxControllerService_.tick();
  xboxControllerService_.setBackgroundReconnectDeferred(teleopContinuousEnabled_.load());
  localInputs_.joystickPaired = xboxControllerService_.isControllerPaired();
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  const bool profileCalibration =
      profile == ControllerOperationProfile::CalibrationLeader ||
      profile == ControllerOperationProfile::CalibrationFollower;
  if (profileCalibration && calibrationEngaged_.load()) {
    localInputs_.calibrationDone = false;
  } else {
    localInputs_.calibrationDone = !config::leader::kCalibrationRequired ||
                                   uptimeMs > config::leader::kCalibrationReadyMs;
  }
  const bool wifiDirectTeleopUp =
      profile == ControllerOperationProfile::TeleopWifi && wifiDirectTeleopActive_.load() &&
      wifiDirectLinkEngaged_.load() && presenceService_->hasValidFollowerIp();
  const bool espNowTeleopSession =
      teleopContinuousEnabled_.load() && isEspNowTeleopProfile(profile) &&
      presenceService_->isPaired();
  localInputs_.espNowLinked =
      presenceService_->isFollowerLinked() || espNowTeleopSession || wifiDirectTeleopUp;
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

} // namespace soarm
