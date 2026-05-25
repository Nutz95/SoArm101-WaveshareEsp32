#include "follower_app.h"
#include "follower_presence_service.h"
#include "../Config/common_runtime_config.h"
#include "../Config/follower_runtime_config.h"
#include "../common/command/command_ack_status.h"
#include "../common/servo/servo_control_opcode.h"

#include <Arduino.h>
#include <cstddef>

// WiFi credentials injected from environment variables at build time.
// See platformio.ini build_flags for WIFI_SSID / WIFI_PASS definitions.
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

#ifndef SERVO_BUS_RX_PIN
#define SERVO_BUS_RX_PIN 18
#endif
#ifndef SERVO_BUS_TX_PIN
#define SERVO_BUS_TX_PIN 19
#endif
#ifndef SERVO_BUS_BAUD
#define SERVO_BUS_BAUD 1000000U
#endif

namespace soarm {

FollowerApp::FollowerApp()
    : statusLedService_(STATUS_LED_PIN, STATUS_LED_COUNT),
      wifiOta_(WIFI_SSID, WIFI_PASS, "soarm-follower"),
      presenceService_(std::unique_ptr<IFollowerPresenceService>(new FollowerPresenceService())),
      localInputs_{false, false, false, false} {}

void FollowerApp::begin() {
  Serial.begin(115200);

  statusLedService_.begin();

  const bool nvsReady = calibrationStore_.begin();
  CalibrationProfile followerProfile{};
  if (!nvsReady || !calibrationStore_.load(ArmRole::Follower, followerProfile)) {
    const CalibrationProfile defaults = calibrationStore_.buildDefaultProfile();
    calibrationStore_.save(ArmRole::Follower, defaults);
  }

  WifiOtaCallbacks cb;
  cb.onWifiConnected    = [](const char *ip) { Serial.printf("[WiFi] connected ip=%s\n", ip); };
  cb.onWifiDisconnected = []() { Serial.println("[WiFi] disconnected"); };
  cb.onOtaBegin         = []() { Serial.println("[OTA] begin"); };
  cb.onOtaEnd           = []() { Serial.println("[OTA] end"); };
  cb.onOtaError         = [](uint32_t code) { Serial.printf("[OTA] error %u\n", code); };
  wifiOta_.begin(cb);

  ServoBusConfig servoBusConfig{};
  servoBusConfig.serial = &Serial2;
  servoBusConfig.rxPin = SERVO_BUS_RX_PIN;
  servoBusConfig.txPin = SERVO_BUS_TX_PIN;
  servoBusConfig.baudRate = SERVO_BUS_BAUD;
  servoBusConfig.firstId = 1U;
  servoBusConfig.lastId = 32U;
  if (!servoBusService_.begin(servoBusConfig)) {
    Serial.println("[WARN] Servo bus init failed on follower");
  }

  const uint8_t startupScanCount = servoBusService_.scan();
  Serial.printf("[SERVO] follower startup scan: %u servo(s)\n", startupScanCount);

  const bool espNowReady = presenceService_->begin();
  if (!espNowReady) {
    Serial.println("[WARN] ESP-NOW init failed on follower");
  }

  publishServoTelemetry();
}

void FollowerApp::tick() {
  wifiOta_.tick();
  presenceService_->tick(wifiOta_.ipAddress());

  processIncomingTeleopBatch();
  processIncomingServoControl();
  processIncomingServoScan();
  publishServoTelemetry();

  updateStateAndLeds(millis());

  delay(config::follower::kTickDelayMs);
}

void FollowerApp::processIncomingTeleopBatch() {
  uint8_t ids[config::common::kTeleopBatchMaxServos]{};
  int16_t positions[config::common::kTeleopBatchMaxServos]{};
  uint8_t count = 0U;
  uint8_t speedPct = 0U;
  uint16_t requestId = 0U;

  if (!presenceService_->consumeTeleopMirrorBatch(
          ids,
          positions,
          config::common::kTeleopBatchMaxServos,
          count,
          speedPct,
          requestId)) {
    return;
  }

  const uint16_t speed = static_cast<uint16_t>(
      (static_cast<uint32_t>(speedPct) * config::follower::kTeleopServoMaxSpeedRaw) / 100U);
  const bool ok = servoBusService_.moveBatch(ids, positions, count, speed);
  presenceService_->updateLastCommandAck(
      requestId,
      static_cast<uint8_t>(ServoControlOpcode::TeleopMirrorBatch),
      static_cast<uint8_t>(ok ? CommandAckStatus::Applied : CommandAckStatus::Failed));
  presenceService_->requestImmediatePresenceTx();
}

void FollowerApp::processIncomingServoControl() {
  uint8_t servoControlOp = 0U;
  uint32_t servoControlValue = 0U;
  uint16_t servoControlRequestId = 0U;
  if (!presenceService_->consumeServoControl(servoControlOp, servoControlValue, servoControlRequestId)) {
    return;
  }

  const CommandAckStatus ackStatus = executeServoControl(servoControlOp, servoControlValue);
  presenceService_->updateLastCommandAck(
      servoControlRequestId,
      servoControlOp,
      static_cast<uint8_t>(ackStatus));
  presenceService_->requestImmediatePresenceTx();
}

void FollowerApp::processIncomingServoScan() {
  uint16_t servoScanRequestId = 0U;
  if (!presenceService_->consumeServoScanRequested(servoScanRequestId)) {
    return;
  }

  const uint32_t scanStartMs = millis();
  const uint8_t foundCount = servoBusService_.scan();
  const uint32_t scanDurationMs = millis() - scanStartMs;
  Serial.printf("[SERVO] scan complete: %u servo(s) found in %lu ms (%s)\n",
                foundCount,
                static_cast<unsigned long>(scanDurationMs),
                servoBusService_.lastScanSummary());
  presenceService_->updateLastCommandAck(
      servoScanRequestId,
      static_cast<uint8_t>(ServoControlOpcode::Scan),
      static_cast<uint8_t>(CommandAckStatus::Applied));
  presenceService_->requestImmediatePresenceTx();
}

void FollowerApp::publishServoTelemetry() {
  servoBusService_.refreshKnownTelemetryFast();
  servoBusService_.pollTemperatureAlarmSlow();
  presenceService_->updateServoTelemetry(
      servoBusService_.lastIdsText(),
      servoBusService_.lastTelemetryText(),
      servoBusService_.lastScanCount(),
      servoBusService_.isDebugManual(),
      servoBusService_.hasTemperatureAlarm());
}

void FollowerApp::updateStateAndLeds(uint32_t uptimeMs) {
  localInputs_.calibrationDone = uptimeMs > config::follower::kCalibrationReadyMs;
  localInputs_.espNowLinked = uptimeMs > config::follower::kEspNowLinkedReadyMs;

  ArmRuntimeState localState = stateMachine_.computeState(localInputs_);
  if (servoBusService_.lastScanCount() != config::common::kExpectedFollowerServoCount) {
    localState = ArmRuntimeState::ServoFault;
  }
  statusLedService_.render(0, localState);
}

CommandAckStatus FollowerApp::executeServoControl(uint8_t op, uint32_t value) {
  using ServoHandler = CommandAckStatus (FollowerApp::*)(uint32_t);
  struct ServoDispatchEntry {
    uint8_t opcode;
    ServoHandler handler;
  };

  static const ServoDispatchEntry kServoDispatchTable[] = {
      {static_cast<uint8_t>(ServoControlOpcode::DebugEnable), &FollowerApp::handleDebugEnable},
      {static_cast<uint8_t>(ServoControlOpcode::DebugDisable), &FollowerApp::handleDebugDisable},
      {static_cast<uint8_t>(ServoControlOpcode::Move), &FollowerApp::handleMove},
      {static_cast<uint8_t>(ServoControlOpcode::TeleopMirror), &FollowerApp::handleTeleopMirror},
      {static_cast<uint8_t>(ServoControlOpcode::SetId), &FollowerApp::handleSetId},
      {static_cast<uint8_t>(ServoControlOpcode::SetMode), &FollowerApp::handleSetMode},
  };

  for (size_t i = 0; i < (sizeof(kServoDispatchTable) / sizeof(kServoDispatchTable[0])); ++i) {
    if (kServoDispatchTable[i].opcode == op) {
      return (this->*kServoDispatchTable[i].handler)(value);
    }
  }
  return CommandAckStatus::Rejected;
}

CommandAckStatus FollowerApp::handleDebugEnable(uint32_t value) {
  (void)value;
  servoBusService_.setDebugManual(true);
  Serial.println("[SERVO] debug manual enabled");
  return CommandAckStatus::Applied;
}

CommandAckStatus FollowerApp::handleDebugDisable(uint32_t value) {
  (void)value;
  servoBusService_.setDebugManual(false);
  Serial.println("[SERVO] debug manual disabled");
  return CommandAckStatus::Applied;
}

CommandAckStatus FollowerApp::handleMove(uint32_t value) {
  const uint8_t id = static_cast<uint8_t>(value & 0xFFU);
  const int16_t position = static_cast<int16_t>((value >> 8U) & 0xFFFFU);
  const uint8_t speedPct = static_cast<uint8_t>((value >> 24U) & 0xFFU);
  const uint16_t speed = static_cast<uint16_t>(
      (static_cast<uint32_t>(speedPct) * config::follower::kTeleopServoMaxSpeedRaw) / 100U);

  if (!servoBusService_.isDebugManual()) {
    return CommandAckStatus::Rejected;
  }

  const bool ok = servoBusService_.moveTo(id, position, speed, 0U);
  Serial.printf("[SERVO] move %s id=%u pos=%d\n", ok ? "ok" : "fail", id, position);
  return ok ? CommandAckStatus::Applied : CommandAckStatus::Failed;
}

CommandAckStatus FollowerApp::handleTeleopMirror(uint32_t value) {
  const uint8_t id = static_cast<uint8_t>(value & 0xFFU);
  const int16_t position = static_cast<int16_t>((value >> 8U) & 0xFFFFU);
  const uint8_t speedPct = static_cast<uint8_t>((value >> 24U) & 0xFFU);
  const uint16_t speed = static_cast<uint16_t>(
      (static_cast<uint32_t>(speedPct) * config::follower::kTeleopServoMaxSpeedRaw) / 100U);

  const bool ok = servoBusService_.moveTo(id, position, speed, 0U);
  Serial.printf("[SERVO] teleop mirror %s id=%u pos=%d\n", ok ? "ok" : "fail", id, position);
  return ok ? CommandAckStatus::Applied : CommandAckStatus::Failed;
}

CommandAckStatus FollowerApp::handleSetId(uint32_t value) {
  const uint8_t oldId = static_cast<uint8_t>(value & 0xFFU);
  const uint8_t newId = static_cast<uint8_t>((value >> 8U) & 0xFFU);

  if (!servoBusService_.isDebugManual()) {
    return CommandAckStatus::Rejected;
  }

  const bool ok = servoBusService_.setServoId(oldId, newId);
  if (ok) {
    servoBusService_.scan();
  }
  Serial.printf("[SERVO] set-id %s %u->%u\n", ok ? "ok" : "fail", oldId, newId);
  return ok ? CommandAckStatus::Applied : CommandAckStatus::Failed;
}

CommandAckStatus FollowerApp::handleSetMode(uint32_t value) {
  const uint8_t id = static_cast<uint8_t>(value & 0xFFU);
  const uint8_t mode = static_cast<uint8_t>((value >> 8U) & 0xFFU);

  if (!servoBusService_.isDebugManual()) {
    return CommandAckStatus::Rejected;
  }

  const bool ok = servoBusService_.setServoMode(id, mode);
  Serial.printf("[SERVO] set-mode %s id=%u mode=%u\n", ok ? "ok" : "fail", id, mode);
  return ok ? CommandAckStatus::Applied : CommandAckStatus::Failed;
}

} // namespace soarm
