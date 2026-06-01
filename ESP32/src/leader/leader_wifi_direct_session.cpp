#include "leader_wifi_direct_session.h"

#include "../Config/leader_runtime_config.h"
#include "../common/usb_debug_log_gate.h"
#include "leader_presence_service.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstring>

namespace soarm {

void LeaderWifiDirectSession::begin(LeaderPresenceService &presence, WifiDirectRadioService &radio) {
  presence_ = &presence;
  radio_ = &radio;
  activeSessionId_ = static_cast<uint32_t>(millis());
  followerReady_ = false;
  followerTeleopIp_[0] = '\0';
  offerSent_ = false;
  offerSentMs_ = 0U;
  lastOfferResendMs_ = 0U;
  presence.clearFollowerWifiDirectState();

  if (!generateWifiDirectCredentials(activeSessionId_, credentials_)) {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] credential generation failed");
    return;
  }

  // Keep AP and ESP-NOW offer on the current radio channel when known.
  // - With router: avoids "peer channel != home channel" send failures.
  // - Without router: preserves the current ESP-NOW pairing channel instead of forcing ch1.
  const uint8_t currentChannel = WiFi.channel();
  if (currentChannel != 0U) {
    credentials_.channel = currentChannel;
  }

  if (!radio.beginAccessPoint(credentials_.ssid, credentials_.psk, credentials_.channel)) {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] AP bring-up failed");
    return;
  }

  (void)sendOffer(presence);
}

void LeaderWifiDirectSession::end(WifiDirectRadioService &radio, bool restoreHomeSta) {
  (void)restoreHomeSta;
  radio.endSession(true);
  if (presence_ != nullptr) {
    presence_->clearFollowerWifiDirectState();
  }
  presence_ = nullptr;
  radio_ = nullptr;
  offerSent_ = false;
  followerReady_ = false;
  followerTeleopIp_[0] = '\0';
  USB_DEBUG_LOGLN("[WIFI-DIRECT] session ended");
}

void LeaderWifiDirectSession::tick(LeaderPresenceService &presence, WifiDirectRadioService &radio, uint32_t nowMs) {
  (void)radio;

  const char *ackIp = presence.followerWifiDirectIp();
  if (!followerReady_ && ackIp != nullptr && ackIp[0] != '\0') {
    strncpy(followerTeleopIp_, ackIp, sizeof(followerTeleopIp_) - 1U);
    followerTeleopIp_[sizeof(followerTeleopIp_) - 1U] = '\0';
    followerReady_ = true;
    USB_DEBUG_LOGF("[WIFI-DIRECT] follower ack ip=%s\n", followerTeleopIp_);
    return;
  }

  if (!followerReady_) {
    handleAckTimeout(presence, nowMs);
  }
}

bool LeaderWifiDirectSession::isActive() const {
  return radio_ != nullptr && radio_->isAccessPointActive();
}

bool LeaderWifiDirectSession::isFollowerReady() const {
  return followerReady_;
}

const char *LeaderWifiDirectSession::followerTeleopIp() const {
  return followerTeleopIp_;
}

const char *LeaderWifiDirectSession::leaderApIp() const {
  if (radio_ == nullptr) {
    return "";
  }
  return radio_->accessPointIp();
}

bool LeaderWifiDirectSession::sendOffer(LeaderPresenceService &presence) {
  WifiDirectOfferPacket packet{};
  buildWifiDirectOfferPacket(credentials_, packet);
  if (!presence.sendWifiDirectOffer(packet)) {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] ESP-NOW offer send failed (paired?)");
    return false;
  }
  offerSent_ = true;
  offerSentMs_ = millis();
  lastOfferResendMs_ = offerSentMs_;
  USB_DEBUG_LOGLN("[WIFI-DIRECT] ESP-NOW offer sent");
  return true;
}

void LeaderWifiDirectSession::handleAckTimeout(LeaderPresenceService &presence, uint32_t nowMs) {
  if (!offerSent_ || offerSentMs_ == 0U) {
    return;
  }

  if ((nowMs - offerSentMs_) >= config::leader::kWifiDirectAckTimeoutMs) {
    USB_DEBUG_LOGLN("[WIFI-DIRECT] follower ack timeout");
    offerSent_ = false;
    offerSentMs_ = 0U;
  }

  if (offerSentMs_ == 0U ||
      (nowMs - lastOfferResendMs_) >= config::leader::kWifiDirectOfferResendMs) {
    (void)sendOffer(presence);
  }
}

} // namespace soarm
