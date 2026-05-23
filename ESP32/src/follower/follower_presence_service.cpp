#include "follower_presence_service.h"

#include "../common/presence/presence_constants.h"
#include "../common/presence/presence_message_type.h"
#include "../common/presence/presence_packet.h"
#include "../common/pairing/pairing_policy.h"
#include "../common/servo/servo_control_opcode.h"

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <cstring>

namespace soarm {

FollowerPresenceService::FollowerPresenceService()
    : pairingStore_("soarm-pair", "leader_mac") {
}

bool FollowerPresenceService::begin() {
  Serial.println("[FOLLOWER] === begin() ===");

  if (!initEspNow()) {
    Serial.println("[FOLLOWER] ESP-NOW init FAILED");
    return false;
  }
  Serial.println("[FOLLOWER] ESP-NOW init OK");

  hasPairedLeaderMac_ = pairingStore_.load(pairedLeaderMac_);
  if (hasPairedLeaderMac_) {
    formatMacAddress(pairedLeaderMac_, pairedLeaderMacText_);
    Serial.printf("[FOLLOWER] NVS: loaded paired leader MAC %s\n", pairedLeaderMacText_);
  } else {
    strncpy(pairedLeaderMacText_, "unpaired", sizeof(pairedLeaderMacText_) - 1);
    pairedLeaderMacText_[sizeof(pairedLeaderMacText_) - 1] = '\0';
    Serial.println("[FOLLOWER] NVS: no paired leader (unpaired)");
  }

  const String localMac = WiFi.macAddress();
  strncpy(localMacText_, localMac.c_str(), sizeof(localMacText_) - 1);
  localMacText_[sizeof(localMacText_) - 1] = '\0';
  Serial.printf("[FOLLOWER] Local MAC: %s\n", localMacText_);

  if (!addBroadcastPeer()) {
    Serial.println("[FOLLOWER] addBroadcastPeer FAILED");
    return false;
  }
  Serial.println("[FOLLOWER] Broadcast peer added OK");

  if (hasPairedLeaderMac_) {
    if (!addPeer(pairedLeaderMac_)) {
      Serial.printf("[FOLLOWER] addPeer(%s) FAILED\n", pairedLeaderMacText_);
      return false;
    }
    Serial.printf("[FOLLOWER] Paired leader peer added: %s\n", pairedLeaderMacText_);
  }

  started_ = true;
  Serial.println("[FOLLOWER] === begin() DONE ===");
  return true;
}

void FollowerPresenceService::tick(const char *localIp) {
  if (!started_) {
    return;
  }

  if (localIp != nullptr && localIp[0] != '\0') {
    strncpy(lastLocalIp_, localIp, sizeof(lastLocalIp_) - 1);
    lastLocalIp_[sizeof(lastLocalIp_) - 1] = '\0';
  }

  const uint32_t nowMs = millis();
  if (!forcePresenceTx_ && ((nowMs - lastTxMs_) < kPresenceTxPeriodMs)) {
    return;
  }

  forcePresenceTx_ = false;
  lastTxMs_ = nowMs;
  if (hasPairedLeader()) {
    Serial.printf("[FOLLOWER] tick: sending Presence to %s\n", pairedLeaderMacText_);
    sendPresence(localIp);

    // Send PairRequest periodically even when paired.
    // This forces re-negotiation if leader reset but follower didn't receive the reset message.
    const uint32_t pairRequestIntervalMs = 5000U;
    if ((nowMs - lastPairRequestMs_) >= pairRequestIntervalMs) {
      lastPairRequestMs_ = nowMs;
      Serial.printf("[FOLLOWER] tick: sending periodic PairRequest to %s\n", pairedLeaderMacText_);
      sendPairRequest(localIp);
    }
  } else {
    // Unpaired: send PairRequest every tick for fast re-pairing.
    Serial.printf("[FOLLOWER] tick: sending PairRequest (unpaired)\n");
    sendPairRequest(localIp);
    lastPairRequestMs_ = nowMs; // Reset interval so we don't double-send
  }
}

bool FollowerPresenceService::isPaired() const {
  return hasPairedLeaderMac_;
}

const char *FollowerPresenceService::pairedPeerMac() const {
  return pairedLeaderMacText_;
}

const char *FollowerPresenceService::localMac() const {
  return localMacText_;
}

bool FollowerPresenceService::resetPairing() {
  Serial.println("[PAIR] resetPairing() called");
  hasPairedLeaderMac_ = false;
  memset(pairedLeaderMac_, 0, sizeof(pairedLeaderMac_));
  strncpy(pairedLeaderMacText_, "unpaired", sizeof(pairedLeaderMacText_) - 1);
  pairedLeaderMacText_[sizeof(pairedLeaderMacText_) - 1] = '\0';
  pairingStore_.clear();
  Serial.println("[PAIR] NVS cleared");

  // Re-add broadcast peer to ensure we can receive PairReset and send PairRequest.
  addBroadcastPeer();
  Serial.println("[PAIR] Broadcast peer re-added");

  // Immediately start sending PairRequest so leader picks us up fast.
  lastPairRequestMs_ = 0;
  return true;
}

bool FollowerPresenceService::consumeServoScanRequested(uint16_t &requestId) {
  const bool requested = servoScanRequested_;
  requestId = pendingServoScanRequestId_;
  servoScanRequested_ = false;
  return requested;
}

bool FollowerPresenceService::consumeServoControl(uint8_t &op, uint32_t &value, uint16_t &requestId) {
  const bool requested = servoControlRequested_;
  if (!requested) {
    return false;
  }

  op = pendingServoControlOp_;
  value = pendingServoControlValue_;
  requestId = pendingServoControlRequestId_;
  servoControlRequested_ = false;
  return true;
}

void FollowerPresenceService::updateLastCommandAck(uint16_t requestId, uint8_t op, uint8_t status) {
  lastAckRequestId_ = requestId;
  lastAckCommandOp_ = op;
  lastAckStatus_ = status;
}

void FollowerPresenceService::requestImmediatePresenceTx() {
  forcePresenceTx_ = true;
}

void FollowerPresenceService::updateServoTelemetry(
    const char *servoIds,
    const char *servoTelemetry,
    uint8_t servoCount,
    bool debugManual) {
  servoCount_ = servoCount;
  servoDebugManual_ = debugManual;

  if (servoIds != nullptr) {
    strncpy(servoIdsText_, servoIds, sizeof(servoIdsText_) - 1);
    servoIdsText_[sizeof(servoIdsText_) - 1] = '\0';
  }
  if (servoTelemetry != nullptr) {
    strncpy(servoTelemetryText_, servoTelemetry, sizeof(servoTelemetryText_) - 1);
    servoTelemetryText_[sizeof(servoTelemetryText_) - 1] = '\0';
  }
}

void FollowerPresenceService::onPresenceFrame(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != static_cast<int>(sizeof(PresencePacket))) {
    return;
  }

  PresencePacket packet{};
  memcpy(&packet, data, sizeof(packet));

  if (packet.magic != kPresenceMagic || packet.version != kPresenceVersion) {
    return;
  }

  const PresenceMessageType msgType = static_cast<PresenceMessageType>(packet.messageType);
  Serial.printf("[FOLLOWER] RX msgType=%u from %02X:%02X:%02X:%02X:%02X:%02X\n",
                static_cast<unsigned>(msgType),
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (msgType == PresenceMessageType::PairReset) {
    Serial.println("[FOLLOWER] >>> PairReset received - clearing pairing");
    resetPairing();
    Serial.println("[FOLLOWER] PairReset complete - now unpaired");
    return;
  }

  if (msgType == PresenceMessageType::ServoScan) {
    Serial.println("[FOLLOWER] >>> ServoScan requested");
    servoScanRequested_ = true;
    return;
  }

  if (msgType == PresenceMessageType::ServoControl) {
    Serial.printf("[FOLLOWER] >>> ServoControl op=%u val=%u\n",
                  static_cast<unsigned>(packet.controlOp), packet.controlValue);
    if (packet.controlOp == static_cast<uint8_t>(ServoControlOpcode::Scan)) {
      servoScanRequested_ = true;
      pendingServoScanRequestId_ = packet.reserved2;
    } else {
      pendingServoControlOp_ = packet.controlOp;
      pendingServoControlValue_ = packet.controlValue;
      pendingServoControlRequestId_ = packet.reserved2;
      servoControlRequested_ = true;
    }
    return;
  }

  if (msgType != PresenceMessageType::PairAck) {
    Serial.printf("[FOLLOWER] Ignored msgType=%u (expected PairAck)\n",
                  static_cast<unsigned>(msgType));
    return;
  }

  Serial.printf("[FOLLOWER] >>> PairAck received from %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (!hasPairedLeaderMac_) {
    Serial.printf("[FOLLOWER] PairAck: was unpaired, now pairing with %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    memcpy(pairedLeaderMac_, mac, sizeof(pairedLeaderMac_));
    hasPairedLeaderMac_ = pairingStore_.save(pairedLeaderMac_);
    if (hasPairedLeaderMac_) {
      formatMacAddress(pairedLeaderMac_, pairedLeaderMacText_);
      Serial.printf("[FOLLOWER] Pairing saved to NVS: %s\n", pairedLeaderMacText_);
      addPeer(pairedLeaderMac_);
      Serial.printf("[FOLLOWER] Peer added to ESP-NOW: %s\n", pairedLeaderMacText_);
    } else {
      Serial.println("[FOLLOWER] Pairing FAILED: NVS save error");
    }
  } else {
    // Already paired. Accept PairAck only from the paired leader MAC.
    const bool macChanged = memcmp(pairedLeaderMac_, mac, sizeof(pairedLeaderMac_)) != 0;
    if (!PairingPolicy::shouldAcceptFollowerPairAck(hasPairedLeaderMac_, !macChanged)) {
      Serial.printf("[FOLLOWER] PairAck: rejected different leader MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
      Serial.printf("[FOLLOWER] PairAck: already paired with same leader, ignoring\n");
    }
  }
}

bool FollowerPresenceService::addBroadcastPeer() {
  uint8_t broadcastAddr[ESP_NOW_ETH_ALEN];
  memset(broadcastAddr, 0xFF, sizeof(broadcastAddr));
  return addPeer(broadcastAddr);
}

bool FollowerPresenceService::addPeer(const uint8_t mac[6]) {
  return EspNowPresenceBase::addPeer(mac);
}

bool FollowerPresenceService::hasPairedLeader() const {
  return hasPairedLeaderMac_;
}

void FollowerPresenceService::sendPairRequest(const char *localIp) {
  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::PairRequest);
  if (localIp != nullptr && localIp[0] != '\0') {
    strncpy(packet.ip, localIp, sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  } else {
    strncpy(packet.ip, "0.0.0.0", sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  }

  if (hasPairedLeaderMac_) {
    esp_now_send(pairedLeaderMac_, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    return;
  }

  uint8_t broadcastAddr[ESP_NOW_ETH_ALEN];
  memset(broadcastAddr, 0xFF, sizeof(broadcastAddr));
  esp_now_send(broadcastAddr, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

void FollowerPresenceService::sendPresence(const char *localIp) {
  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::Presence);
  if (localIp != nullptr && localIp[0] != '\0') {
    strncpy(packet.ip, localIp, sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  } else if (lastLocalIp_[0] != '\0') {
    strncpy(packet.ip, lastLocalIp_, sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  } else {
    strncpy(packet.ip, "0.0.0.0", sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  }

  packet.servoCount = servoCount_;
  packet.reserved = lastAckStatus_;
  packet.reserved2 = lastAckRequestId_;
  packet.controlOp = servoDebugManual_ ? 1U : 0U;
  packet.controlValue = static_cast<uint32_t>(lastAckCommandOp_);
  strncpy(packet.servoIds, servoIdsText_, sizeof(packet.servoIds) - 1);
  packet.servoIds[sizeof(packet.servoIds) - 1] = '\0';
  strncpy(packet.servoTelemetry, servoTelemetryText_, sizeof(packet.servoTelemetry) - 1);
  packet.servoTelemetry[sizeof(packet.servoTelemetry) - 1] = '\0';

  esp_now_send(pairedLeaderMac_, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

} // namespace soarm
