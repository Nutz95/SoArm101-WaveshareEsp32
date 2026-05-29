#include "follower_presence_service.h"

#include "../common/command/command_ack_status.h"
#include "../common/pairing/pairing_policy.h"
#include "../common/presence/presence_constants.h"
#include "../common/presence/presence_message_type.h"
#include "../common/servo/servo_control_opcode.h"
#include "../Config/common_runtime_config.h"

#include <Arduino.h>
#include <cstring>

namespace soarm {

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
  if (msgType != PresenceMessageType::ServoControlBatch) {
    Serial.printf("[FOLLOWER] RX msgType=%u from %02X:%02X:%02X:%02X:%02X:%02X\n",
                  static_cast<unsigned>(msgType),
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

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
}

void FollowerPresenceService::handlePairResetMessage(const uint8_t *mac, const PresencePacket &packet) {
  (void)mac;
  (void)packet;
  handlePairResetFrame();
}

void FollowerPresenceService::handleServoScanMessage(const uint8_t *mac, const PresencePacket &packet) {
  (void)mac;
  (void)packet;
  Serial.println("[FOLLOWER] ServoScan requested");
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
  Serial.println("[FOLLOWER] PairReset - clearing pairing");
  resetPairing();
}

void FollowerPresenceService::handleServoControlFrame(const PresencePacket &packet) {
  Serial.printf("[FOLLOWER] ServoControl op=%u req=%u\n",
                static_cast<unsigned>(packet.controlOp),
                static_cast<unsigned>(packet.reserved2));

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
    sendCommandAck(
        packet.reserved2,
        packet.controlOp,
        static_cast<uint8_t>(CommandAckStatus::Rejected),
        packet.reserved);
  }
}

void FollowerPresenceService::handleServoControlBatchFrame(const PresencePacket &packet) {
  if (packet.controlOp != static_cast<uint8_t>(ServoControlOpcode::TeleopMirrorBatch)) {
    return;
  }

  PendingTeleopBatch batch{};
  const uint8_t rawCount = static_cast<uint8_t>(packet.servoTelemetry[0]);
  const uint8_t clampedCount = (rawCount > config::common::kTeleopBatchMaxServos)
                                   ? config::common::kTeleopBatchMaxServos
                                   : rawCount;
  batch.count = clampedCount;
  batch.speedPct = static_cast<uint8_t>(packet.servoTelemetry[1]);
  batch.requestId = packet.reserved2;

  for (uint8_t i = 0U; i < clampedCount; ++i) {
    const uint8_t offset = static_cast<uint8_t>(2U + (i * 3U));
    batch.ids[i] = static_cast<uint8_t>(packet.servoTelemetry[offset]);
    const uint16_t lo = static_cast<uint8_t>(packet.servoTelemetry[offset + 1U]);
    const uint16_t hi = static_cast<uint8_t>(packet.servoTelemetry[offset + 2U]);
    batch.positions[i] = static_cast<int16_t>((hi << 8U) | lo);
  }

  lastTeleopBatchRxMs_ = millis();
  enqueueTeleopBatch(batch);
}

void FollowerPresenceService::handlePairAckFrame(const uint8_t *mac) {
  if (!hasPairedLeaderMac_) {
    memcpy(pairedLeaderMac_, mac, sizeof(pairedLeaderMac_));
    hasPairedLeaderMac_ = pairingStore_.save(pairedLeaderMac_);
    if (hasPairedLeaderMac_) {
      formatMacAddress(pairedLeaderMac_, pairedLeaderMacText_);
      addPeer(pairedLeaderMac_);
      Serial.printf("[FOLLOWER] Paired with leader %s\n", pairedLeaderMacText_);
    } else {
      Serial.println("[FOLLOWER] Pairing NVS save failed");
    }
    return;
  }

  const bool macChanged = memcmp(pairedLeaderMac_, mac, sizeof(pairedLeaderMac_)) != 0;
  if (!PairingPolicy::shouldAcceptFollowerPairAck(hasPairedLeaderMac_, !macChanged)) {
    Serial.printf("[FOLLOWER] PairAck rejected (different leader)\n");
  }
}

} // namespace soarm
