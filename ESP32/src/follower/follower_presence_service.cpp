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
  if (!initEspNow()) {
    Serial.println("[FOLLOWER] ESP-NOW init failed");
    return false;
  }

  hasPairedLeaderMac_ = pairingStore_.load(pairedLeaderMac_);
  if (hasPairedLeaderMac_) {
    formatMacAddress(pairedLeaderMac_, pairedLeaderMacText_);
  } else {
    strncpy(pairedLeaderMacText_, "unpaired", sizeof(pairedLeaderMacText_) - 1);
    pairedLeaderMacText_[sizeof(pairedLeaderMacText_) - 1] = '\0';
  }

  const String localMac = WiFi.macAddress();
  strncpy(localMacText_, localMac.c_str(), sizeof(localMacText_) - 1);
  localMacText_[sizeof(localMacText_) - 1] = '\0';

  if (!addBroadcastPeer()) {
    return false;
  }

  if (hasPairedLeaderMac_ && !addPeer(pairedLeaderMac_)) {
    return false;
  }

  started_ = true;
  Serial.printf("[FOLLOWER] presence ready paired=%d mac=%s\n",
                static_cast<int>(hasPairedLeaderMac_),
                pairedLeaderMacText_);
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
  const bool teleopActive =
      lastTeleopBatchRxMs_ > 0U && ((nowMs - lastTeleopBatchRxMs_) < config::follower::kTeleopTrafficRecentMs);
  const uint32_t presencePeriodMs =
      teleopActive ? config::follower::kPresenceTxPeriodTeleopMs : kPresenceTxPeriodMs;

  if (!forcePresenceTx_ && ((nowMs - lastTxMs_) < presencePeriodMs)) {
    return;
  }

  forcePresenceTx_ = false;
  lastTxMs_ = nowMs;
  if (hasPairedLeader()) {
    sendPresence(localIp);

    const uint32_t pairRequestIntervalMs =
        teleopActive ? config::follower::kPairRequestIntervalTeleopMs : config::follower::kPairRequestIntervalMs;
    if ((nowMs - lastPairRequestMs_) >= pairRequestIntervalMs) {
      lastPairRequestMs_ = nowMs;
      sendPairRequest(localIp);
    }
  } else {
    sendPairRequest(localIp);
    lastPairRequestMs_ = nowMs;
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
  if (ids == nullptr || positions == nullptr || capacity == 0U) {
    return false;
  }

  PendingTeleopBatch batch{};
  if (!dequeueTeleopBatch(batch)) {
    return false;
  }

  const uint8_t copyCount = (batch.count < capacity) ? batch.count : capacity;
  for (uint8_t i = 0U; i < copyCount; ++i) {
    ids[i] = batch.ids[i];
    positions[i] = batch.positions[i];
  }

  count = copyCount;
  speedPct = batch.speedPct;
  requestId = batch.requestId;
  return true;
}

void FollowerPresenceService::enqueueTeleopBatch(const PendingTeleopBatch &batch) {
  teleopBatchQueue_[0] = batch;
  teleopBatchQueueHead_ = 0U;
  teleopBatchQueueTail_ = 1U;
  teleopBatchQueueCount_ = 1U;
}

bool FollowerPresenceService::dequeueTeleopBatch(PendingTeleopBatch &batch) {
  if (teleopBatchQueueCount_ == 0U) {
    return false;
  }
  batch = teleopBatchQueue_[teleopBatchQueueHead_];
  teleopBatchQueueHead_ = static_cast<uint8_t>((teleopBatchQueueHead_ + 1U) % config::follower::kTeleopBatchQueueCapacity);
  --teleopBatchQueueCount_;
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

void FollowerPresenceService::stageTeleopBatchAck(uint16_t requestId, uint8_t status) {
  lastAckRequestId_ = requestId;
  lastAckCommandOp_ = static_cast<uint8_t>(ServoControlOpcode::TeleopMirrorBatch);
  lastAckStatus_ = status;
}

void FollowerPresenceService::requestImmediatePresenceTx() {
  forcePresenceTx_ = true;
}

void FollowerPresenceService::sendLinkKeepalive(const char *localIp) {
  if (!started_ || !hasPairedLeaderMac_) {
    return;
  }
  sendPresence(localIp);
  lastTxMs_ = millis();
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

} // namespace soarm
