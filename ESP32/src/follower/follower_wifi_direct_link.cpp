#include "follower_wifi_direct_link.h"

#include "../Config/follower_runtime_config.h"
#include "../common/usb_debug_log_gate.h"
#include "../common/wifi_ota_service.h"
#include "follower_presence_service.h"

#include <WiFi.h>
#include <cstring>

namespace soarm {

void FollowerWifiDirectLink::acceptOffer(const WifiDirectCredentials &credentials) {
  credentials_ = credentials;
  offerPending_ = true;
  staReady_ = false;
  connectStartedMs_ = 0U;
  USB_DEBUG_LOGF("[WIFI-DIRECT] offer queued ssid=%s\n", credentials_.ssid);
}

void FollowerWifiDirectLink::reset(
    FollowerPresenceService &presence,
    WifiDirectRadioService &radio,
    WifiOtaService &wifiOta,
    bool restoreHomeSta) {
  presence.setDirectWifiSessionActive(false);
  (void)wifiOta;
  radio.endSession(true);
  if (restoreHomeSta && wifiOta.isStaConnectDesired() && WiFi.status() != WL_CONNECTED) {
    // WifiOtaService will reconnect on next tick when STA is desired.
  }
  offerPending_ = false;
  staReady_ = false;
  connectStartedMs_ = 0U;
  credentials_ = WifiDirectCredentials{};
  USB_DEBUG_LOGLN("[WIFI-DIRECT] link reset");
}

bool FollowerWifiDirectLink::isActive() const {
  return offerPending_ || staReady_;
}

void FollowerWifiDirectLink::tick(
    FollowerPresenceService &presence,
    WifiDirectRadioService &radio,
    WifiOtaService &wifiOta,
    uint32_t nowMs) {
  (void)wifiOta;
  radio.tickStationLink();

  if (staReady_ && WiFi.status() != WL_CONNECTED) {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] lost AP, restoring router STA");
    reset(presence, radio, wifiOta, true);
    return;
  }

  if (!offerPending_) {
    return;
  }

  if (connectStartedMs_ == 0U) {
    if (!radio.beginStation(credentials_.ssid, credentials_.psk)) {
      offerPending_ = false;
      USB_DEBUG_LOGLN("[WIFI-DIRECT] STA start failed");
      return;
    }
    connectStartedMs_ = nowMs;
  }

  if (WiFi.status() == WL_CONNECTED) {
    const char *ip = radio.stationIp();
    if (ip != nullptr && ip[0] != '\0') {
      staReady_ = true;
      offerPending_ = false;
      trySendAck(presence, radio);
      return;
    }
  }

  if ((nowMs - connectStartedMs_) >= config::follower::kWifiDirectStaConnectTimeoutMs) {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] STA connect timeout");
    reset(presence, radio, wifiOta, true);
  }
}

void FollowerWifiDirectLink::trySendAck(FollowerPresenceService &presence, WifiDirectRadioService &radio) {
  const char *ip = radio.stationIp();
  if (ip == nullptr || ip[0] == '\0') {
    return;
  }
  if (presence.sendWifiDirectAck(credentials_.sessionId, WifiDirectAckStatus::Connected, ip)) {
    presence.setDirectWifiSessionActive(true);
    USB_DEBUG_LOGF("[WIFI-DIRECT] ESP-NOW ack sent ip=%s\n", ip);
  } else {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] ESP-NOW ack send failed");
  }
}

} // namespace soarm
