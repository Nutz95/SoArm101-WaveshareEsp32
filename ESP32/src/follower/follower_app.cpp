#include "follower_app.h"
#include "follower_presence_service.h"

#include <Arduino.h>

// WiFi credentials injected from environment variables at build time.
// See platformio.ini build_flags for WIFI_SSID / WIFI_PASS definitions.
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
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
  cb.onWifiDisconnected = []()               { Serial.println("[WiFi] disconnected"); };
  cb.onOtaBegin         = []()               { Serial.println("[OTA] begin"); };
  cb.onOtaEnd           = []()               { Serial.println("[OTA] end"); };
  cb.onOtaError         = [](uint32_t code)  { Serial.printf("[OTA] error %u\n", code); };
  wifiOta_.begin(cb);

  const bool espNowReady = presenceService_->begin();
  if (!espNowReady) {
    Serial.println("[WARN] ESP-NOW init failed on follower");
  }
}

void FollowerApp::tick() {
  wifiOta_.tick();
  presenceService_->tick(wifiOta_.ipAddress());

  const uint32_t uptimeMs = millis();

  localInputs_.calibrationDone = uptimeMs > 5000U;
  localInputs_.espNowLinked    = uptimeMs > 8000U;

  const ArmRuntimeState localState = stateMachine_.computeState(localInputs_);
  statusLedService_.render(0, localState);

  delay(25);
}

} // namespace soarm
