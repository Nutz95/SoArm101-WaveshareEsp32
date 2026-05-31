#include "leader_app.h"

#include "../Config/leader_runtime_config.h"

#ifndef LEADER_SERVO_BUS_RX_PIN
#define LEADER_SERVO_BUS_RX_PIN 18
#endif
#ifndef LEADER_SERVO_BUS_TX_PIN
#define LEADER_SERVO_BUS_TX_PIN 19
#endif
#ifndef LEADER_SERVO_BUS_BAUD
#define LEADER_SERVO_BUS_BAUD 1000000U
#endif
#include "../common/controller/controller_operation_profile.h"
#include "leader_ble_config.h"
#include "../common/teleop/teleop_wifi_packet.h"

namespace soarm {

void LeaderApp::runDeferredBootStages(uint32_t uptimeMs) {
  if (deferredBootComplete_) {
    return;
  }

  const uint32_t sinceBootMs = uptimeMs - bootMs_;

#ifndef LEADER_SAFE_BOOT
  if (!deferredOledReady_ && sinceBootMs >= 100U) {
    Serial.println("[BOOT] stage: oled");
    if (!oled_.begin()) {
      Serial.println("[WARN] OLED not found");
    } else {
      oled_.showConnecting(followerIpHint_);
    }
    deferredOledReady_ = true;
  }

  if (!deferredServoBusReady_ && sinceBootMs >= config::leader::kDeferredServoBusMs) {
    Serial.println("[BOOT] stage: servo bus");
    ServoBusConfig servoBusConfig{};
    servoBusConfig.serial = &Serial2;
    servoBusConfig.rxPin = LEADER_SERVO_BUS_RX_PIN;
    servoBusConfig.txPin = LEADER_SERVO_BUS_TX_PIN;
    servoBusConfig.baudRate = LEADER_SERVO_BUS_BAUD;
    servoBusConfig.firstId = 1U;
    servoBusConfig.lastId = 32U;
    if (!servoBusService_.begin(servoBusConfig)) {
      Serial.println("[WARN] Servo bus init failed on leader");
    } else {
      const uint8_t localScanCount = servoBusService_.scan();
      leaderStartupScanDone_ = true;
      leaderServoFault_ = localScanCount != config::common::kExpectedLeaderServoCount;
      Serial.printf("[BOOT] servo scan count=%u\n", static_cast<unsigned>(localScanCount));
    }
    deferredServoBusReady_ = true;
  }

  if (!deferredNetworkReady_ && sinceBootMs >= config::leader::kDeferredNetworkMs && deferredServoBusReady_) {
    Serial.println("[BOOT] stage: network services");
    if (!telemetryStreamServer_.begin(9090)) {
      Serial.println("[WARN] Telemetry stream init failed");
    } else {
      Serial.println("[INFO] Telemetry stream on :9090");
    }
    if (!teleopWifiBridge_.begin(static_cast<uint16_t>(teleop_wifi::kFollowerListenPort + 1U))) {
      Serial.println("[WARN] Teleop Wi-Fi UDP bridge init failed on leader");
    }
    teleopPcSerialBridge_.attach(Serial);
    syncWifiRadioPolicyForProfile(
        sanitizeControllerOperationProfile(controllerOperationProfile_.load()));
    deferredNetworkReady_ = true;
  }

  if (!deferredBackgroundTasksReady_ && sinceBootMs >= config::leader::kDeferredBackgroundTasksMs &&
      deferredServoBusReady_) {
    Serial.println("[BOOT] stage: background tasks");
    startBackgroundTasks();
    deferredBackgroundTasksReady_ = true;
  }

#if !LEADER_ENABLE_XBOX_BLE
  if (!deferredBleReady_) {
    deferredBleReady_ = true;
  }
#endif

  if (deferredOledReady_ && deferredServoBusReady_ && deferredNetworkReady_ && deferredBackgroundTasksReady_ &&
      deferredBleReady_) {
    deferredBootComplete_ = true;
    Serial.println("[BOOT] leader deferred init complete");
  }
#else
  (void)sinceBootMs;
  if (!deferredBootComplete_) {
    deferredBootComplete_ = true;
    Serial.println("[BOOT] LEADER_SAFE_BOOT: BLE/servo/network deferred init disabled");
  }
#endif
}

} // namespace soarm
