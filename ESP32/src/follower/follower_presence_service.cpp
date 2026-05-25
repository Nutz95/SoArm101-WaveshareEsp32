#include "follower_presence_service.h"

#include "../common/command/command_ack_status.h"
#include "../common/presence/presence_constants.h"
#include "../common/presence/presence_message_type.h"
#include "../common/presence/presence_packet.h"
#include "../common/pairing/pairing_policy.h"
#include "../common/servo/servo_control_opcode.h"
#include "../Config/common_runtime_config.h"

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
    if ((nowMs - lastPairRequestMs_) >= config::follower::kPairRequestIntervalMs) {
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
  if (requested) {
    lastConsumedControlSequence_ = pendingServoScanSequence_;
  }
  servoScanRequested_ = false;
  return requested;
}

bool FollowerPresenceService::consumeServoControl(uint8_t &op, uint32_t &value, uint16_t &requestId) {
  uint8_t sequence = 0U;
  const bool dequeued = dequeueServoControl(op, value, requestId, sequence);
  if (!dequeued) {
    return false;
  }

  lastConsumedControlSequence_ = sequence;
  hasLastProcessedControl_ = true;
  lastProcessedOp_ = op;
  lastProcessedValue_ = value;
  lastProcessedRequestId_ = requestId;
  lastProcessedSequence_ = sequence;
  return true;
}

bool FollowerPresenceService::consumeTeleopMirrorBatch(
    uint8_t *ids,
    int16_t *positions,
    uint8_t capacity,
    uint8_t &count,
    uint8_t &speedPct,
    uint16_t &requestId) {
  if (ids == nullptr || positions == nullptr || capacity == 0U || !teleopBatchPending_) {
    return false;
  }

  const uint8_t copyCount = (teleopBatchCount_ < capacity) ? teleopBatchCount_ : capacity;
  for (uint8_t i = 0U; i < copyCount; ++i) {
    ids[i] = teleopBatchIds_[i];
    positions[i] = teleopBatchPositions_[i];
  }

  count = copyCount;
  speedPct = teleopBatchSpeedPct_;
  requestId = teleopBatchRequestId_;
  teleopBatchPending_ = false;
  return true;
}

void FollowerPresenceService::updateLastCommandAck(uint16_t requestId, uint8_t op, uint8_t status) {
  lastAckRequestId_ = requestId;
  lastAckCommandOp_ = op;
  lastAckStatus_ = status;
  if (op == static_cast<uint8_t>(ServoControlOpcode::Scan)) {
    hasLastProcessedScan_ = true;
    lastProcessedScanRequestId_ = requestId;
    lastProcessedScanSequence_ = lastConsumedControlSequence_;
  }
  sendCommandAck(requestId, op, status, lastConsumedControlSequence_);
}

void FollowerPresenceService::requestImmediatePresenceTx() {
  forcePresenceTx_ = true;
}

void FollowerPresenceService::updateServoTelemetry(
    const char *servoIds,
    const char *servoTelemetry,
    uint8_t servoCount,
    bool debugManual,
    bool temperatureAlarm) {
  servoCount_ = servoCount;
  servoDebugManual_ = debugManual;
  servoTemperatureAlarm_ = temperatureAlarm;

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

  using FrameHandler = void (FollowerPresenceService::*)(const uint8_t *, const PresencePacket &);
  struct DispatchEntry {
    PresenceMessageType messageType;
    FrameHandler handler;
  };

  static const DispatchEntry kDispatchTable[] = {
      {PresenceMessageType::PairReset, &FollowerPresenceService::handlePairResetMessage},
      {PresenceMessageType::ServoScan, &FollowerPresenceService::handleServoScanMessage},
      {PresenceMessageType::ServoControl, &FollowerPresenceService::handleServoControlMessage},
      {PresenceMessageType::ServoControlBatch, &FollowerPresenceService::handleServoControlBatchMessage},
      {PresenceMessageType::PairAck, &FollowerPresenceService::handlePairAckMessage},
  };

  for (const DispatchEntry &entry : kDispatchTable) {
    if (entry.messageType == msgType) {
      (this->*entry.handler)(mac, packet);
      return;
    }
  }

  Serial.printf("[FOLLOWER] Ignored msgType=%u\n", static_cast<unsigned>(msgType));
}

void FollowerPresenceService::handlePairResetMessage(const uint8_t *mac, const PresencePacket &packet) {
  (void)mac;
  (void)packet;
  handlePairResetFrame();
}

void FollowerPresenceService::handleServoScanMessage(const uint8_t *mac, const PresencePacket &packet) {
  (void)mac;
  (void)packet;
  Serial.println("[FOLLOWER] >>> ServoScan requested");
  servoScanRequested_ = true;
}

void FollowerPresenceService::handleServoControlMessage(const uint8_t *mac, const PresencePacket &packet) {
  (void)mac;
  handleServoControlFrame(packet);
}

void FollowerPresenceService::handleServoControlBatchMessage(const uint8_t *mac, const PresencePacket &packet) {
  (void)mac;
  handleServoControlBatchFrame(packet);
}

void FollowerPresenceService::handlePairAckMessage(const uint8_t *mac, const PresencePacket &packet) {
  (void)packet;
  handlePairAckFrame(mac);
}

void FollowerPresenceService::handlePairResetFrame() {
  Serial.println("[FOLLOWER] >>> PairReset received - clearing pairing");
  resetPairing();
  Serial.println("[FOLLOWER] PairReset complete - now unpaired");
}

void FollowerPresenceService::handleServoControlFrame(const PresencePacket &packet) {
  Serial.printf("[FOLLOWER] >>> ServoControl op=%u val=%u req=%u seq=%u\n",
                static_cast<unsigned>(packet.controlOp),
                packet.controlValue,
                static_cast<unsigned>(packet.reserved2),
                static_cast<unsigned>(packet.reserved));

  if (isDuplicateControlFrame(packet.controlOp, packet.controlValue, packet.reserved2, packet.reserved)) {
    if (lastAckRequestId_ == packet.reserved2 && lastAckCommandOp_ == packet.controlOp) {
      sendCommandAck(lastAckRequestId_, lastAckCommandOp_, lastAckStatus_, packet.reserved);
    }
    return;
  }

  if (packet.controlOp == static_cast<uint8_t>(ServoControlOpcode::Scan)) {
    const bool duplicatePending =
        servoScanRequested_ &&
        pendingServoScanRequestId_ == packet.reserved2 &&
        pendingServoScanSequence_ == packet.reserved;
    const bool duplicateProcessed =
        hasLastProcessedScan_ &&
        lastProcessedScanRequestId_ == packet.reserved2 &&
        lastProcessedScanSequence_ == packet.reserved;
    if (duplicatePending || duplicateProcessed) {
      if (lastAckRequestId_ == packet.reserved2 &&
          lastAckCommandOp_ == static_cast<uint8_t>(ServoControlOpcode::Scan)) {
        sendCommandAck(lastAckRequestId_, lastAckCommandOp_, lastAckStatus_, packet.reserved);
      }
      return;
    }

    servoScanRequested_ = true;
    pendingServoScanRequestId_ = packet.reserved2;
    pendingServoScanSequence_ = packet.reserved;
    return;
  }

  const bool queued = enqueueServoControl(packet.controlOp, packet.controlValue, packet.reserved2, packet.reserved);
  if (!queued) {
    sendCommandAck(packet.reserved2,
                   packet.controlOp,
                   static_cast<uint8_t>(CommandAckStatus::Rejected),
                   packet.reserved);
  }
}

void FollowerPresenceService::handleServoControlBatchFrame(const PresencePacket &packet) {
  if (packet.controlOp != static_cast<uint8_t>(ServoControlOpcode::TeleopMirrorBatch)) {
    return;
  }

  const uint8_t rawCount = static_cast<uint8_t>(packet.servoTelemetry[0]);
  const uint8_t clampedCount = (rawCount > config::common::kTeleopBatchMaxServos)
                                   ? config::common::kTeleopBatchMaxServos
                                   : rawCount;
  teleopBatchCount_ = clampedCount;
  teleopBatchSpeedPct_ = static_cast<uint8_t>(packet.servoTelemetry[1]);
  teleopBatchRequestId_ = packet.reserved2;

  for (uint8_t i = 0U; i < clampedCount; ++i) {
    const uint8_t offset = static_cast<uint8_t>(2U + (i * 3U));
    teleopBatchIds_[i] = static_cast<uint8_t>(packet.servoTelemetry[offset]);
    const uint16_t lo = static_cast<uint8_t>(packet.servoTelemetry[offset + 1U]);
    const uint16_t hi = static_cast<uint8_t>(packet.servoTelemetry[offset + 2U]);
    teleopBatchPositions_[i] = static_cast<int16_t>((hi << 8U) | lo);
  }

  teleopBatchPending_ = true;
}

void FollowerPresenceService::handlePairAckFrame(const uint8_t *mac) {
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
    return;
  }

  const bool macChanged = memcmp(pairedLeaderMac_, mac, sizeof(pairedLeaderMac_)) != 0;
  if (!PairingPolicy::shouldAcceptFollowerPairAck(hasPairedLeaderMac_, !macChanged)) {
    Serial.printf("[FOLLOWER] PairAck: rejected different leader MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    Serial.printf("[FOLLOWER] PairAck: already paired with same leader, ignoring\n");
  }
}

} // namespace soarm
