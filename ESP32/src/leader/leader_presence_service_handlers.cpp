#include "leader_presence_service.h"

#include "../common/pairing/pairing_policy.h"
#include "../common/presence/presence_message_type.h"
#include "../common/wifi/wifi_direct_session.h"
#include "../common/usb_debug_log_gate.h"

#include <Arduino.h>
#include <cstring>
#include <cstring>

namespace soarm {

void LeaderPresenceService::handlePairRequest(const uint8_t *mac, const PresencePacket &packet) {
  (void)packet;
  USB_DEBUG_LOGF("[PAIR] RX PairRequest from %02X:%02X:%02X:%02X:%02X:%02X (paired=%d)\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                   static_cast<int>(hasPairedMac_));

  if (!hasPairedMac_) {
    memcpy(pairedFollowerMac_, mac, sizeof(pairedFollowerMac_));
    hasPairedMac_ = pairingStore_.save(pairedFollowerMac_);
    if (hasPairedMac_) {
      formatMacAddress(pairedFollowerMac_, pairedFollowerMacText_);
      USB_DEBUG_LOGF("[PAIR] NEW pairing saved: %s\n", pairedFollowerMacText_);
    }
    (void)ensureEspNowTransportReady();
    addPeer(mac);
    sendPairAck(mac);
    USB_DEBUG_LOGF("[PAIR] PairAck sent to %02X:%02X:%02X:%02X:%02X:%02X\n",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    linkHeartbeat_.notifyPeerActivity(millis());
    return;
  }

  if (PairingPolicy::shouldAcceptLeaderPairRequest(hasPairedMac_, isPairedMac(mac))) {
    (void)ensureEspNowTransportReady();
    addPeer(mac);
    sendPairAck(mac);
    USB_DEBUG_LOGF("[PAIR] PairAck sent to paired peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    linkHeartbeat_.notifyPeerActivity(millis());
  } else {
    USB_DEBUG_LOGF("[PAIR] Reject PairRequest from unknown peer %02X:%02X:%02X:%02X:%02X:%02X (paired=%d)\n",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                     static_cast<int>(hasPairedMac_));
    sendPairResetTo(mac);
  }
}

void LeaderPresenceService::handlePresenceData(const uint8_t *mac, const PresencePacket &packet) {
  if (!hasPairedMac_) {
    handlePairRequest(mac, packet);
    if (!hasPairedMac_) {
      return;
    }
  }

  if (!isPairedMac(mac)) {
    USB_DEBUG_LOGF("[PAIR] RX Presence from unknown peer while paired: %02X:%02X:%02X:%02X:%02X:%02X\n",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    sendPairResetTo(mac);
    return;
  }

  strncpy(followerIp_, packet.ip, sizeof(followerIp_) - 1);
  followerIp_[sizeof(followerIp_) - 1] = '\0';
  followerServoCount_ = packet.servoCount;
  const bool debugBitInControlValue = ((packet.controlValue >> 8U) & 0x01U) != 0U;
  const bool tempAlarmBitInControlValue = ((packet.controlValue >> 9U) & 0x01U) != 0U;
  followerServoDebugManual_ = debugBitInControlValue || (packet.controlOp != 0U);
  followerServoTemperatureAlarm_ = tempAlarmBitInControlValue;
  followerLastAckStatus_ = packet.reserved;
  followerLastAckRequestId_ = packet.reserved2;
  followerLastAckCommandOp_ = static_cast<uint8_t>(packet.controlValue & 0xFFU);
  followerLastAckMs_ = millis();
  strncpy(followerServoIdsText_, packet.servoIds, sizeof(followerServoIdsText_) - 1);
  followerServoIdsText_[sizeof(followerServoIdsText_) - 1] = '\0';
  strncpy(followerServoTelemetryText_, packet.servoTelemetry, sizeof(followerServoTelemetryText_) - 1);
  followerServoTelemetryText_[sizeof(followerServoTelemetryText_) - 1] = '\0';
}

void LeaderPresenceService::handleLinkHeartbeat(const uint8_t *mac, const LinkHeartbeatPacket &packet) {
  if (!hasPairedMac_) {
    return;
  }

  if (!isPairedMac(mac)) {
    return;
  }

  strncpy(followerIp_, packet.ip, sizeof(followerIp_) - 1);
  followerIp_[sizeof(followerIp_) - 1] = '\0';
  followerServoCount_ = packet.servoCount;
  followerLastAckStatus_ = packet.ackStatus;
  followerLastAckRequestId_ = packet.ackRequestId;
  followerLastAckCommandOp_ = packet.ackCommandOp;
  followerLastAckMs_ = millis();
}

void LeaderPresenceService::handleWifiDirectAck(const uint8_t *mac, const WifiDirectAckPacket &packet) {
  if (!hasPairedMac_ || !isPairedMac(mac)) {
    return;
  }
  if (!validateWifiDirectAckPacket(packet, followerWifiDirectSessionId_)) {
    return;
  }
  if (packet.status != static_cast<uint8_t>(WifiDirectAckStatus::Connected)) {
    return;
  }
  if (packet.followerStaIp[0] == '\0') {
    return;
  }

  strncpy(followerWifiDirectIp_, packet.followerStaIp, sizeof(followerWifiDirectIp_) - 1U);
  followerWifiDirectIp_[sizeof(followerWifiDirectIp_) - 1U] = '\0';
  strncpy(followerIp_, packet.followerStaIp, sizeof(followerIp_) - 1U);
  followerIp_[sizeof(followerIp_) - 1U] = '\0';
  linkHeartbeat_.notifyPeerActivity(millis());
}

const char *LeaderPresenceService::followerWifiDirectIp() const {
  return followerWifiDirectIp_;
}

void LeaderPresenceService::clearFollowerWifiDirectState() {
  followerWifiDirectIp_[0] = '\0';
  followerWifiDirectSessionId_ = 0U;
}

void LeaderPresenceService::resetTurboTeleopSession() {
  turboEncodeSession_.reset();
}

bool LeaderPresenceService::takeTeleopLoadFeedbackRx(int8_t loads[6], uint16_t &requestId, uint8_t &seq) {
  if (!pendingLoadFeedbackReady_ || loads == nullptr) {
    return false;
  }
  memcpy(loads, pendingLoadFeedbackLoads_, sizeof(pendingLoadFeedbackLoads_));
  requestId = pendingLoadFeedbackRequestId_;
  seq = pendingLoadFeedbackSeq_;
  pendingLoadFeedbackReady_ = false;
  return true;
}

uint32_t LeaderPresenceService::teleopLoadFeedbackTimeoutCount() const {
  return teleopLoadFeedbackTimeoutCount_;
}

void LeaderPresenceService::handleTeleopLoadFeedbackFrame(
    const uint8_t *mac,
    const uint8_t *data,
    size_t len) {
  if (!hasPairedMac_ || !isPairedMac(mac) || data == nullptr) {
    return;
  }

  uint16_t requestId = 0U;
  uint8_t seq = 0U;
  if (!teleop_load_feedback::decodePacket(data, len, requestId, seq, pendingLoadFeedbackLoads_)) {
    return;
  }

  pendingLoadFeedbackRequestId_ = requestId;
  pendingLoadFeedbackSeq_ = seq;
  pendingLoadFeedbackReady_ = true;
}

void LeaderPresenceService::handleServoCommandAck(const uint8_t *mac, const PresencePacket &packet) {
  if (!hasPairedMac_ || !isPairedMac(mac)) {
    return;
  }

  followerLastAckStatus_ = packet.reserved;
  followerLastAckRequestId_ = packet.reserved2;
  followerLastAckCommandOp_ = packet.controlOp;
  const bool debugBitInControlValue = ((packet.controlValue >> 8U) & 0x01U) != 0U;
  const bool tempAlarmBitInControlValue = ((packet.controlValue >> 9U) & 0x01U) != 0U;
  followerServoDebugManual_ = debugBitInControlValue;
  followerServoTemperatureAlarm_ = tempAlarmBitInControlValue;
  followerLastAckMs_ = millis();
  linkHeartbeat_.notifyPeerActivity(millis());
}

} // namespace soarm
