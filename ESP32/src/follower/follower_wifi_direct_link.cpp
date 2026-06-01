#include "follower_wifi_direct_link.h"

#include "../Config/follower_runtime_config.h"
#include "../common/usb_debug_log_gate.h"
#include "../common/wifi_ota_service.h"
#include "follower_presence_service.h"

#include <WiFi.h>
#include <cstring>

namespace soarm {

namespace {

constexpr uint8_t kMaxStaJoinAttempts = 3U;

bool sameCredentials(const WifiDirectCredentials &left, const WifiDirectCredentials &right) {
  return left.sessionId == right.sessionId &&
         strncmp(left.ssid, right.ssid, sizeof(left.ssid)) == 0;
}

} // namespace

bool FollowerWifiDirectLink::shouldAcceptOffer(const WifiDirectCredentials &credentials) const {
  if (!sameCredentials(credentials_, credentials)) {
    return true;
  }
  return !(offerPending_ || staReady_ || connectStartedMs_ != 0U);
}

void FollowerWifiDirectLink::acceptOffer(const WifiDirectCredentials &credentials) {
  if (!shouldAcceptOffer(credentials)) {
    return;
  }

  credentials_ = credentials;
  offerPending_ = true;
  staReady_ = false;
  ackSent_ = false;
  connectStartedMs_ = 0U;
  joinAttemptCount_ = 0U;
  USB_DEBUG_LOGF("[WIFI-DIRECT] offer queued ssid=%s session=%lu\n",
                 credentials_.ssid,
                 static_cast<unsigned long>(credentials_.sessionId));
}

void FollowerWifiDirectLink::reset(
    FollowerPresenceService &presence,
    WifiDirectRadioService &radio,
    WifiOtaService &wifiOta,
    bool restoreHomeSta) {
  (void)restoreHomeSta;
  (void)wifiOta;
  presence.setDirectWifiSessionActive(false);
  presence.clearWifiDirectJoinSession();
  radio.endSession(true);
  (void)presence.ensureEspNowTransportReady();
  offerPending_ = false;
  staReady_ = false;
  ackSent_ = false;
  connectStartedMs_ = 0U;
  joinAttemptCount_ = 0U;
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

  if (staReady_) {
    if (!ackSent_) {
      const char *ip = radio.stationIp();
      if (ip == nullptr || ip[0] == '\0') {
        const String localIp = WiFi.localIP().toString();
        if (localIp.length() > 0U && localIp != "0.0.0.0") {
          ip = localIp.c_str();
        }
      }
      if (ip != nullptr && ip[0] != '\0') {
        trySendAck(presence, radio, ip);
      }
    }
    return;
  }

  if (!offerPending_) {
    return;
  }

  if (connectStartedMs_ == 0U) {
    if (!radio.beginStation(credentials_.ssid, credentials_.psk)) {
      offerPending_ = false;
      presence.clearWifiDirectJoinSession();
      USB_DEBUG_LOGLN("[WIFI-DIRECT] STA start failed");
      return;
    }
    connectStartedMs_ = nowMs;
    ++joinAttemptCount_;
    const uint8_t channel = credentials_.channel == 0U ? 1U : credentials_.channel;
    (void)presence.ensureEspNowTransportReady(channel);
    USB_DEBUG_LOGF("[WIFI-DIRECT] STA join attempt %u ssid=%s\n",
                   static_cast<unsigned>(joinAttemptCount_),
                   credentials_.ssid);
  }

  if (WiFi.status() == WL_CONNECTED) {
    const char *ip = radio.stationIp();
    if (ip == nullptr || ip[0] == '\0') {
      const String localIp = WiFi.localIP().toString();
      if (localIp.length() > 0U && localIp != "0.0.0.0") {
        ip = localIp.c_str();
      }
    }
    if (ip != nullptr && ip[0] != '\0') {
      staReady_ = true;
      offerPending_ = false;
      joinAttemptCount_ = 0U;
      if (!ackSent_) {
        trySendAck(presence, radio, ip);
      }
      return;
    }
  }

  if ((nowMs - connectStartedMs_) < config::follower::kWifiDirectStaConnectTimeoutMs) {
    return;
  }

  USB_DEBUG_LOGLN("[WIFI-DIRECT] STA connect timeout");
  radio.endSession(true);
  connectStartedMs_ = 0U;
  (void)presence.ensureEspNowTransportReady(credentials_.channel == 0U ? 1U : credentials_.channel);

  if (joinAttemptCount_ >= kMaxStaJoinAttempts) {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] STA join giving up");
    reset(presence, radio, wifiOta, true);
    return;
  }

  USB_DEBUG_LOGLN("[WIFI-DIRECT] STA join retry scheduled");
}

void FollowerWifiDirectLink::trySendAck(
    FollowerPresenceService &presence,
    WifiDirectRadioService &radio,
    const char *stationIp) {
  (void)radio;
  const char *ip = stationIp;
  if (ip == nullptr || ip[0] == '\0') {
    return;
  }
  const uint8_t channel = credentials_.channel == 0U ? 1U : credentials_.channel;
  if (!presence.ensureEspNowTransportReady(channel)) {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] ESP-NOW restore failed before ack");
    return;
  }
  if (presence.sendWifiDirectAck(credentials_.sessionId, WifiDirectAckStatus::Connected, ip)) {
    presence.setDirectWifiSessionActive(true);
    ackSent_ = true;
    USB_DEBUG_LOGF("[WIFI-DIRECT] ESP-NOW ack sent ip=%s\n", ip);
  } else {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] ESP-NOW ack send failed");
  }
}

} // namespace soarm
