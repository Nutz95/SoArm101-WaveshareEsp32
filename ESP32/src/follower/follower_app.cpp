#include "follower_app.h"
#include "follower_presence_service.h"
#include "../common/servo/servo_control_opcode.h"

#include <Arduino.h>

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

  const bool espNowReady = presenceService_->begin();
  if (!espNowReady) {
    Serial.println("[WARN] ESP-NOW init failed on follower");
  }
}

void FollowerApp::tick() {
  wifiOta_.tick();
  presenceService_->tick(wifiOta_.ipAddress());

  uint8_t servoControlOp = 0U;
  uint32_t servoControlValue = 0U;
  if (presenceService_->consumeServoControl(servoControlOp, servoControlValue)) {
    const ServoControlOpcode op = static_cast<ServoControlOpcode>(servoControlOp);
    switch (op) {
    case ServoControlOpcode::DebugEnable:
      servoBusService_.setDebugManual(true);
      Serial.println("[SERVO] debug manual enabled");
      break;
    case ServoControlOpcode::DebugDisable:
      servoBusService_.setDebugManual(false);
      Serial.println("[SERVO] debug manual disabled");
      break;
    case ServoControlOpcode::Move: {
      const uint8_t id = static_cast<uint8_t>(servoControlValue & 0xFFU);
      const int16_t position = static_cast<int16_t>((servoControlValue >> 8U) & 0xFFFFU);
      const uint8_t speedPct = static_cast<uint8_t>((servoControlValue >> 24U) & 0xFFU);
      const uint16_t speed = static_cast<uint16_t>(speedPct) * 10U;
      if (servoBusService_.isDebugManual()) {
        const bool ok = servoBusService_.moveTo(id, position, speed, 0U);
        Serial.printf("[SERVO] move %s id=%u pos=%d\n", ok ? "ok" : "fail", id, position);
      }
      break;
    }
    case ServoControlOpcode::SetId: {
      const uint8_t oldId = static_cast<uint8_t>(servoControlValue & 0xFFU);
      const uint8_t newId = static_cast<uint8_t>((servoControlValue >> 8U) & 0xFFU);
      if (servoBusService_.isDebugManual()) {
        const bool ok = servoBusService_.setServoId(oldId, newId);
        Serial.printf("[SERVO] set-id %s %u->%u\n", ok ? "ok" : "fail", oldId, newId);
      }
      break;
    }
    case ServoControlOpcode::SetMode: {
      const uint8_t id = static_cast<uint8_t>(servoControlValue & 0xFFU);
      const uint8_t mode = static_cast<uint8_t>((servoControlValue >> 8U) & 0xFFU);
      if (servoBusService_.isDebugManual()) {
        const bool ok = servoBusService_.setServoMode(id, mode);
        Serial.printf("[SERVO] set-mode %s id=%u mode=%u\n", ok ? "ok" : "fail", id, mode);
      }
      break;
    }
    case ServoControlOpcode::None:
    case ServoControlOpcode::Scan:
    default:
      break;
    }
  }

  if (presenceService_->consumeServoScanRequested()) {
    const uint8_t foundCount = servoBusService_.scan();
    Serial.printf("[SERVO] scan complete: %u servo(s) found (%s)\n", foundCount, servoBusService_.lastScanSummary());
  }

  presenceService_->updateServoTelemetry(
      servoBusService_.lastIdsText(),
      servoBusService_.lastTelemetryText(),
      servoBusService_.lastScanCount(),
      servoBusService_.isDebugManual());

  const uint32_t uptimeMs = millis();

  localInputs_.calibrationDone = uptimeMs > 5000U;
  localInputs_.espNowLinked    = uptimeMs > 8000U;

  const ArmRuntimeState localState = stateMachine_.computeState(localInputs_);
  statusLedService_.render(0, localState);

  delay(25);
}

} // namespace soarm
