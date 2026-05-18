#include "leader_app.h"
#include "leader_presence_service.h"

#include <Arduino.h>
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

  const uint32_t uptimeMs = millis();

  localInputs_.joystickPaired  = uptimeMs > 3000U;
  localInputs_.calibrationDone = uptimeMs > 6000U;
  localInputs_.espNowLinked    = presenceService_->isFollowerLinked();

  const bool followerIpValid = presenceService_->hasValidFollowerIp();

  ArmRuntimeState localState = stateMachine_.computeState(localInputs_);

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

  if (localInputs_.espNowLinked) {
    followerState_ = ArmRuntimeState::Ready;
    strncpy(followerIpHint_, presenceService_->followerIp(), sizeof(followerIpHint_) - 1);
    followerIpHint_[sizeof(followerIpHint_) - 1] = '\0';
  } else {
    followerState_ = ArmRuntimeState::WaitingEspNow;
    if (!followerIpValid) {
      strncpy(followerIpHint_, "0.0.0.0", sizeof(followerIpHint_) - 1);
      followerIpHint_[sizeof(followerIpHint_) - 1] = '\0';
    }
  }

  statusLedService_.render(0, localState);
  if (STATUS_LED_COUNT > 1U) {
    statusLedService_.render(1, followerState_);
  }

  LeaderTelemetrySnapshot snapshot{};
  snapshot.uptimeMs = uptimeMs;
  // Runtime stats hooks can be plugged here to compute per-core load.
  snapshot.cpu0LoadPct = 0;
  snapshot.cpu1LoadPct = 0;
  snapshot.reserved0 = 0;
  snapshot.reserved1 = 0;
  strncpy(snapshot.leaderIp, wifiOta_.ipAddress(), sizeof(snapshot.leaderIp) - 1);
  snapshot.leaderIp[sizeof(snapshot.leaderIp) - 1] = '\0';
  strncpy(snapshot.followerIp, followerIpHint_, sizeof(snapshot.followerIp) - 1);
  snapshot.followerIp[sizeof(snapshot.followerIp) - 1] = '\0';
  snapshot.leaderState = localState;
  snapshot.followerState = followerState_;
  snapshot.mode = mode_;
  strncpy(snapshot.status, statusLine_, sizeof(snapshot.status) - 1);
  snapshot.status[sizeof(snapshot.status) - 1] = '\0';
  snapshot.joystickPaired = localInputs_.joystickPaired;
  snapshot.calibrationDone = localInputs_.calibrationDone;
  snapshot.espNowLinked = localInputs_.espNowLinked;
  telemetryState_.update(snapshot);

  // Refresh OLED at 5 Hz to improve readability and reduce visual noise.
  if ((uptimeMs - lastOledRefreshMs_) >= oledConfig_.refreshPeriodMs) {
    lastOledRefreshMs_ = uptimeMs;
    if (wifiOta_.isOtaInProgress()) {
      strncpy(statusLine_, "ota updating", sizeof(statusLine_) - 1);
      statusLine_[sizeof(statusLine_) - 1] = '\0';
      oled_.showOtaProgress(50);
    } else {
      oled_.showDashboard(wifiOta_.ipAddress(), followerIpHint_, mode_, statusLine_, uptimeMs);
    }
  }

  delay(25);
}

} // namespace soarm
