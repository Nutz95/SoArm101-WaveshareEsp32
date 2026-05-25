#include "leader_presence_service.h"

#include "../common/presence/presence_constants.h"
#include "../common/presence/presence_message_type.h"
#include "../common/presence/presence_packet.h"
#include "../common/pairing/pairing_policy.h"
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

void LeaderPresenceService::tick() {
  if (hasPairedMac_ && lastFollowerSeenMs_ > 0U) {
    const uint32_t nowMs = millis();
    if ((nowMs - lastFollowerSeenMs_) >= config::leader::kPairingTimeoutMs) {
      Serial.printf("[PAIR] Timeout: no contact from follower for %lu ms, expiring pairing\n",
                    (unsigned long)(nowMs - lastFollowerSeenMs_));
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
  const uint32_t nowMs = millis();
  return hasValidFollowerIp() && ((nowMs - lastFollowerSeenMs_) <= kPresenceTimeoutMs);
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
  lastFollowerSeenMs_ = 0;
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
  if (hasPairedMac_) {
    return sendServoControl(pairedFollowerMac_, static_cast<uint8_t>(ServoControlOpcode::Scan), 0U, requestId);
  }
  return sendServoControlBroadcast(static_cast<uint8_t>(ServoControlOpcode::Scan), 0U, requestId);
}

bool LeaderPresenceService::requestServoControl(uint8_t op, uint32_t value, uint16_t requestId) {
  if (hasPairedMac_) {
    return sendServoControl(pairedFollowerMac_, op, value, requestId);
  }
  return sendServoControlBroadcast(op, value, requestId);
}

void LeaderPresenceService::onPresenceFrame(const uint8_t *mac, const uint8_t *data, int len) {
  if (!started_) {
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

  using FrameHandler = void (LeaderPresenceService::*)(const uint8_t *, const PresencePacket &);
  struct DispatchEntry {
    PresenceMessageType type;
    FrameHandler handler;
  };
  static const DispatchEntry kDispatchTable[] = {
      {PresenceMessageType::PairRequest, &LeaderPresenceService::handlePairRequest},
      {PresenceMessageType::Presence,    &LeaderPresenceService::handlePresenceData},
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

void LeaderPresenceService::handlePairRequest(const uint8_t *mac, const PresencePacket &packet) {
  (void)packet;
  Serial.printf("[PAIR] RX PairRequest from %02X:%02X:%02X:%02X:%02X:%02X (paired=%d)\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                static_cast<int>(hasPairedMac_));

  if (!hasPairedMac_) {
    memcpy(pairedFollowerMac_, mac, sizeof(pairedFollowerMac_));
    hasPairedMac_ = pairingStore_.save(pairedFollowerMac_);
    if (hasPairedMac_) {
      formatMacAddress(pairedFollowerMac_, pairedFollowerMacText_);
      Serial.printf("[PAIR] NEW pairing saved: %s\n", pairedFollowerMacText_);
    }
    addPeer(mac);
    sendPairAck(mac);
    Serial.printf("[PAIR] PairAck sent to %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return;
  }

  if (PairingPolicy::shouldAcceptLeaderPairRequest(hasPairedMac_, isPairedMac(mac))) {
    addPeer(mac);
    sendPairAck(mac);
    Serial.printf("[PAIR] PairAck sent to paired peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    Serial.printf("[PAIR] Reject PairRequest from unknown peer %02X:%02X:%02X:%02X:%02X:%02X (paired=%d)\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                  static_cast<int>(hasPairedMac_));
    sendPairResetTo(mac);
  }
}

void LeaderPresenceService::handlePresenceData(const uint8_t *mac, const PresencePacket &packet) {
  if (!hasPairedMac_) {
    Serial.printf("[PAIR] RX Presence from stale peer while unpaired: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    sendPairResetTo(mac);
    return;
  }

  if (!isPairedMac(mac)) {
    Serial.printf("[PAIR] RX Presence from unknown peer while paired: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    sendPairResetTo(mac);
    return;
  }

  strncpy(followerIp_, packet.ip, sizeof(followerIp_) - 1);
  followerIp_[sizeof(followerIp_) - 1] = '\0';
  followerServoCount_ = packet.servoCount;
  const bool debugBitInControlValue = ((packet.controlValue >> 8U) & 0x01U) != 0U;
  followerServoDebugManual_ = debugBitInControlValue || (packet.controlOp != 0U);
  followerLastAckStatus_ = packet.reserved;
  followerLastAckRequestId_ = packet.reserved2;
  followerLastAckCommandOp_ = static_cast<uint8_t>(packet.controlValue & 0xFFU);
  followerLastAckMs_ = millis();
  strncpy(followerServoIdsText_, packet.servoIds, sizeof(followerServoIdsText_) - 1);
  followerServoIdsText_[sizeof(followerServoIdsText_) - 1] = '\0';
  strncpy(followerServoTelemetryText_, packet.servoTelemetry, sizeof(followerServoTelemetryText_) - 1);
  followerServoTelemetryText_[sizeof(followerServoTelemetryText_) - 1] = '\0';
  lastFollowerSeenMs_ = millis();
}

void LeaderPresenceService::handleServoCommandAck(const uint8_t *mac, const PresencePacket &packet) {
  if (!hasPairedMac_ || !isPairedMac(mac)) {
    return;
  }

  followerLastAckStatus_ = packet.reserved;
  followerLastAckRequestId_ = packet.reserved2;
  followerLastAckCommandOp_ = packet.controlOp;
  const bool debugBitInControlValue = ((packet.controlValue >> 8U) & 0x01U) != 0U;
  followerServoDebugManual_ = debugBitInControlValue;
  followerLastAckMs_ = millis();
  lastFollowerSeenMs_ = followerLastAckMs_;
}

void LeaderPresenceService::sendPairAck(const uint8_t mac[6]) {
  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::PairAck);
  strncpy(packet.ip, WiFi.localIP().toString().c_str(), sizeof(packet.ip) - 1);
  packet.ip[sizeof(packet.ip) - 1] = '\0';

  esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

void LeaderPresenceService::sendPairResetTo(const uint8_t mac[6]) {
  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::PairReset);
  strncpy(packet.ip, WiFi.localIP().toString().c_str(), sizeof(packet.ip) - 1);
  packet.ip[sizeof(packet.ip) - 1] = '\0';

  addPeer(mac);

  Serial.printf("[PAIR] Sending PairReset to %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // esp_now_send is non-blocking. Sending 3 times improves delivery probability.
  esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

void LeaderPresenceService::sendPairResetBroadcast() {
  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::PairReset);
  strncpy(packet.ip, WiFi.localIP().toString().c_str(), sizeof(packet.ip) - 1);
  packet.ip[sizeof(packet.ip) - 1] = '\0';

  uint8_t broadcastAddr[ESP_NOW_ETH_ALEN];
  memset(broadcastAddr, 0xFF, sizeof(broadcastAddr));
  addPeer(broadcastAddr);
  esp_now_send(broadcastAddr, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

void LeaderPresenceService::sendServoScanBroadcast(uint16_t requestId) {
  sendServoControlBroadcast(static_cast<uint8_t>(ServoControlOpcode::Scan), 0U, requestId);
}

bool LeaderPresenceService::sendServoControl(const uint8_t mac[6], uint8_t op, uint32_t value, uint16_t requestId) {
  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::ServoControl);
  packet.reserved = static_cast<uint8_t>(requestId & 0xFFU);
  packet.controlOp = op;
  packet.reserved2 = requestId;
  packet.controlValue = value;
  strncpy(packet.ip, WiFi.localIP().toString().c_str(), sizeof(packet.ip) - 1);
  packet.ip[sizeof(packet.ip) - 1] = '\0';

  const uint8_t sendAttempts = (op == static_cast<uint8_t>(ServoControlOpcode::TeleopMirror))
                                   ? 1U
                                   : config::leader::kFollowerInitialSendBurstCount;

  bool sent = false;
  for (uint8_t attempt = 0U; attempt < sendAttempts; ++attempt) {
    if (esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet)) == ESP_OK) {
      sent = true;
    }
  }
  return sent;
}

bool LeaderPresenceService::sendServoControlBroadcast(uint8_t op, uint32_t value, uint16_t requestId) {
  uint8_t broadcastAddr[ESP_NOW_ETH_ALEN];
  memset(broadcastAddr, 0xFF, sizeof(broadcastAddr));
  if (!addPeer(broadcastAddr)) {
    return false;
  }
  return sendServoControl(broadcastAddr, op, value, requestId);
}

bool LeaderPresenceService::addBroadcastPeer() {
  uint8_t broadcastAddr[ESP_NOW_ETH_ALEN];
  memset(broadcastAddr, 0xFF, sizeof(broadcastAddr));
  return addPeer(broadcastAddr);
}

bool LeaderPresenceService::isPairedMac(const uint8_t mac[6]) const {
  if (!hasPairedMac_) {
    return false;
  }
  return memcmp(pairedFollowerMac_, mac, sizeof(pairedFollowerMac_)) == 0;
}

} // namespace soarm
