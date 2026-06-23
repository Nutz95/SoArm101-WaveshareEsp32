#include "leader_presence_service.h"

#include "../common/link/link_constants.h"
#include "../common/presence/link_heartbeat_packet.h"
#include "../common/presence/presence_constants.h"
#include "../common/presence/presence_message_type.h"
#include "../common/presence/presence_packet.h"
#include "../common/wifi/wifi_direct_offer_packet.h"
#include "../common/wifi/wifi_direct_session.h"
#include "../common/pairing/pairing_policy.h"
#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <cstring>

namespace soarm {

LeaderPresenceService::LeaderPresenceService()
    : pairingStore_("soarm-pair", "follower_mac") {
}

bool LeaderPresenceService::begin() {
  if (!initEspNow()) {
    return false;
  }

  hasPairedMac_ = pairingStore_.load(pairedFollowerMac_);
  if (hasPairedMac_) {
    formatMacAddress(pairedFollowerMac_, pairedFollowerMacText_);
  } else {
    strncpy(pairedFollowerMacText_, "unpaired", sizeof(pairedFollowerMacText_) - 1);
    pairedFollowerMacText_[sizeof(pairedFollowerMacText_) - 1] = '\0';
  }

  const String localMac = WiFi.macAddress();
  strncpy(localMacText_, localMac.c_str(), sizeof(localMacText_) - 1);
  localMacText_[sizeof(localMacText_) - 1] = '\0';

  if (!addBroadcastPeer()) {
    return false;
  }

  started_ = true;
  return true;
}

void LeaderPresenceService::setPairingWatchdogSuspended(bool suspended) {
  if (pairingWatchdogSuspended_ && !suspended && hasPairedMac_ && hasValidFollowerIp()) {
    linkHeartbeat_.notifyPeerActivity(millis());
  }
  pairingWatchdogSuspended_ = suspended;
}

void LeaderPresenceService::notifyPeerLinkActivity() {
  if (hasPairedMac_) {
    linkHeartbeat_.notifyPeerActivity(millis());
  }
}

void LeaderPresenceService::refreshFollowerLinkGrace() {
  notifyPeerLinkActivity();
}

void LeaderPresenceService::tick() {
  if (!pairingWatchdogSuspended_ && hasPairedMac_ && linkHeartbeat_.lastPeerActivityMs() > 0U) {
    const uint32_t nowMs = millis();
    if (!linkHeartbeat_.isPeerAlive(nowMs, config::leader::kPairingTimeoutMs)) {
      Serial.printf("[PAIR] Timeout: no contact from follower for %lu ms, expiring pairing\n",
                    (unsigned long)(nowMs - linkHeartbeat_.lastPeerActivityMs()));
      hasPairedMac_ = false;
      memset(pairedFollowerMac_, 0, sizeof(pairedFollowerMac_));
      strncpy(pairedFollowerMacText_, "unpaired", sizeof(pairedFollowerMacText_) - 1);
      pairedFollowerMacText_[sizeof(pairedFollowerMacText_) - 1] = '\0';
      followerIp_[0] = '\0';
      pairingStore_.clear();
    }
  }

  // Drain scheduled PairReset broadcasts one per tick to avoid blocking.
  if (pendingResetBroadcastCount_ > 0U) {
    const uint32_t nowMs = millis();
    if (nowMs >= nextResetBroadcastMs_) {
      sendPairResetBroadcast();
      pendingResetBroadcastCount_ -= 1U;
      nextResetBroadcastMs_ = nowMs + config::leader::kResetBroadcastIntervalMs;
    }
  }
}

bool LeaderPresenceService::isFollowerLinked() const {
  if (!hasPairedMac_) {
    return false;
  }
  // ESP-NOW link liveness must not depend on router IP (salon / no-home-WiFi).
  return linkHeartbeat_.isPeerAlive(millis(), link::kPeerAliveTimeoutMs);
}

bool LeaderPresenceService::canCommandPairedFollower() const {
  if (!hasPairedMac_) {
    return false;
  }
  const uint32_t nowMs = millis();
  if (linkHeartbeat_.isPeerAlive(nowMs, link::kPeerAliveTimeoutMs)) {
    return true;
  }
  // Paired in NVS and we had traffic before (e.g. teleop without router IP).
  return linkHeartbeat_.lastPeerActivityMs() > 0U;
}

bool LeaderPresenceService::isFollowerAvailable() const {
  return hasPairedMac_ && hasValidFollowerIp();
}

bool LeaderPresenceService::hasValidFollowerIp() const {
  if (followerIp_[0] == '\0') {
    return false;
  }
  return strcmp(followerIp_, "0.0.0.0") != 0;
}

const char *LeaderPresenceService::followerIp() const {
  return followerIp_;
}

bool LeaderPresenceService::isPaired() const {
  return hasPairedMac_;
}

const char *LeaderPresenceService::pairedPeerMac() const {
  return pairedFollowerMacText_;
}

const char *LeaderPresenceService::localMac() const {
  return localMacText_;
}

const char *LeaderPresenceService::followerServoIds() const {
  return followerServoIdsText_;
}

const char *LeaderPresenceService::followerServoTelemetry() const {
  return followerServoTelemetryText_;
}

uint8_t LeaderPresenceService::followerServoCount() const {
  return followerServoCount_;
}

bool LeaderPresenceService::followerServoDebugManual() const {
  return followerServoDebugManual_;
}

bool LeaderPresenceService::followerServoTemperatureAlarm() const {
  return followerServoTemperatureAlarm_;
}

uint16_t LeaderPresenceService::followerLastAckRequestId() const {
  return followerLastAckRequestId_;
}

uint8_t LeaderPresenceService::followerLastAckCommandOp() const {
  return followerLastAckCommandOp_;
}

uint8_t LeaderPresenceService::followerLastAckStatus() const {
  return followerLastAckStatus_;
}

uint32_t LeaderPresenceService::followerLastAckMs() const {
  return followerLastAckMs_;
}

bool LeaderPresenceService::resetPairing() {
  uint8_t previousPairMac[6]{};
  const bool wasPaired = hasPairedMac_;
  if (wasPaired) {
    memcpy(previousPairMac, pairedFollowerMac_, sizeof(previousPairMac));
    Serial.printf("[PAIR] Reset: was paired to %02X:%02X:%02X:%02X:%02X:%02X\n",
                  previousPairMac[0], previousPairMac[1], previousPairMac[2],
                  previousPairMac[3], previousPairMac[4], previousPairMac[5]);
  } else {
    Serial.println("[PAIR] Reset: was already unpaired");
  }

  hasPairedMac_ = false;
  memset(pairedFollowerMac_, 0, sizeof(pairedFollowerMac_));
  strncpy(pairedFollowerMacText_, "unpaired", sizeof(pairedFollowerMacText_) - 1);
  pairedFollowerMacText_[sizeof(pairedFollowerMacText_) - 1] = '\0';
  followerIp_[0] = '\0';
  followerServoIdsText_[0] = '\0';
  followerServoTelemetryText_[0] = '\0';
  followerServoCount_ = 0U;
  followerServoDebugManual_ = false;
  followerServoTemperatureAlarm_ = false;
  linkHeartbeat_.reset();
  pairingStore_.clear();
  Serial.println("[PAIR] NVS cleared, now unpaired");

  // Send PairReset to previous peer (unicast) so it knows to clear its state too.
  if (wasPaired) {
    sendPairResetTo(previousPairMac);
  }

  // Broadcast PairReset: send first frame immediately, schedule 2 more via tick().
  sendPairResetBroadcast();
  pendingResetBroadcastCount_ = 2U;
  nextResetBroadcastMs_ = millis() + config::leader::kResetBroadcastIntervalMs;
  Serial.println("[PAIR] PairReset broadcast sent, 2 more scheduled");

  // Force re-accept of any PairRequest immediately by ensuring broadcast peer is present.
  addBroadcastPeer();
  return true;
}

bool LeaderPresenceService::requestServoScan(uint16_t requestId) {
  bool sent = false;
  if (hasPairedMac_) {
    sent = sendServoControl(pairedFollowerMac_, static_cast<uint8_t>(ServoControlOpcode::Scan), 0U, requestId);
  } else {
    sent = sendServoControlBroadcast(static_cast<uint8_t>(ServoControlOpcode::Scan), 0U, requestId);
  }
  if (sent) {
    notifyPeerLinkActivity();
  }
  return sent;
}

bool LeaderPresenceService::requestServoControl(uint8_t op, uint32_t value, uint16_t requestId) {
  if (!started_) {
    return false;
  }

  // Channel 0 keeps peers on the current radio channel. Pinning WiFi.channel() after STA
  // suspend caused ESP-NOW jitter on cold boot until a profile cycle refreshed peers.
  if (!ensureEspNowTransportReady(0U)) {
    return false;
  }

  bool sent = false;
  if (hasPairedMac_) {
    sent = sendServoControl(pairedFollowerMac_, op, value, requestId);
  } else {
    sent = sendServoControlBroadcast(op, value, requestId);
  }
  if (sent) {
    notifyPeerLinkActivity();
  }
  return sent;
}

bool LeaderPresenceService::requestTeleopMirrorBatch(
    const uint8_t *ids,
    const int16_t *positions,
    uint8_t count,
    uint8_t speedPct,
    uint16_t requestId,
    bool turbo) {
  if (ids == nullptr || positions == nullptr || count == 0U) {
    return false;
  }

  if (hasPairedMac_) {
    const bool sent =
        sendServoControlBatch(pairedFollowerMac_, ids, positions, count, speedPct, requestId, turbo);
    if (sent) {
      notifyPeerLinkActivity();
    }
    return sent;
  }

  return false;
}

void LeaderPresenceService::onPresenceFrame(const uint8_t *mac, const uint8_t *data, int len) {
  if (!started_ || data == nullptr) {
    return;
  }

  const uint32_t nowMs = millis();

  if (len == static_cast<int>(sizeof(WifiDirectAckPacket))) {
    WifiDirectAckPacket ackPacket{};
    memcpy(&ackPacket, data, sizeof(ackPacket));
    if (ackPacket.magic != kPresenceMagic || ackPacket.version != kWifiDirectPacketVersion) {
      return;
    }
    if (ackPacket.messageType != static_cast<uint8_t>(PresenceMessageType::WifiDirectAck)) {
      return;
    }
    linkHeartbeat_.notifyPeerActivity(nowMs);
    handleWifiDirectAck(mac, ackPacket);
    return;
  }

  if (len == static_cast<int>(sizeof(LinkHeartbeatPacket))) {
    LinkHeartbeatPacket hbPacket{};
    memcpy(&hbPacket, data, sizeof(hbPacket));
    if (hbPacket.magic != kPresenceMagic || hbPacket.version != kPresenceVersion) {
      return;
    }
    if (hbPacket.messageType != static_cast<uint8_t>(PresenceMessageType::LinkHeartbeat)) {
      return;
    }
    linkHeartbeat_.notifyPeerActivity(nowMs);
    handleLinkHeartbeat(mac, hbPacket);
    return;
  }

  if (len != static_cast<int>(sizeof(PresencePacket))) {
    return;
  }

  PresencePacket packet{};
  memcpy(&packet, data, sizeof(packet));

  if (packet.magic != kPresenceMagic || packet.version != kPresenceVersion) {
    return;
  }

  linkHeartbeat_.notifyPeerActivity(nowMs);

  using FrameHandler = void (LeaderPresenceService::*)(const uint8_t *, const PresencePacket &);
  struct DispatchEntry {
    PresenceMessageType type;
    FrameHandler handler;
  };
  static const DispatchEntry kDispatchTable[] = {
      {PresenceMessageType::PairRequest, &LeaderPresenceService::handlePairRequest},
      {PresenceMessageType::Presence, &LeaderPresenceService::handlePresenceData},
      {PresenceMessageType::ServoCommandAck, &LeaderPresenceService::handleServoCommandAck},
  };

  const PresenceMessageType msgType = static_cast<PresenceMessageType>(packet.messageType);
  for (const DispatchEntry &entry : kDispatchTable) {
    if (entry.type == msgType) {
      (this->*entry.handler)(mac, packet);
      return;
    }
  }
  // ServoScan and ServoControl frames addressed to leader are silently ignored.
}

} // namespace soarm
