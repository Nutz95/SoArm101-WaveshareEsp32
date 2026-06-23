#include "follower_presence_service.h"

#include "../common/command/command_ack_status.h"
#include "../common/link/link_constants.h"
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

bool FollowerPresenceService::preferWifiStaConnected(uint32_t nowMs) const {
  const bool espNowTeleopActive =
      lastTeleopBatchRxMs_ > 0U && ((nowMs - lastTeleopBatchRxMs_) < config::follower::kTeleopTrafficRecentMs);
  const bool wifiTeleopActive =
      lastWifiTeleopRxMs_ > 0U && ((nowMs - lastWifiTeleopRxMs_) < config::follower::kTeleopTrafficRecentMs);
  return !(espNowTeleopActive && !wifiTeleopActive);
}

bool FollowerPresenceService::isTeleopTrafficActive(uint32_t nowMs) const {
  const bool espNowTeleopActive =
      lastTeleopBatchRxMs_ > 0U && ((nowMs - lastTeleopBatchRxMs_) < config::follower::kTeleopTrafficRecentMs);
  const bool wifiTeleopActive =
      lastWifiTeleopRxMs_ > 0U && ((nowMs - lastWifiTeleopRxMs_) < config::follower::kTeleopTrafficRecentMs);
  return espNowTeleopActive || wifiTeleopActive;
}

void FollowerPresenceService::setDirectWifiSessionActive(bool active) {
  directWifiSessionActive_ = active;
  if (!active) {
    pendingWifiDirectSessionEnd_ = false;
    activeWifiDirectSessionId_ = 0U;
    wifiDirectJoinSessionId_ = 0U;
  }
}

void FollowerPresenceService::setWifiDirectJoinSession(uint32_t sessionId) {
  wifiDirectJoinSessionId_ = sessionId;
  activeWifiDirectSessionId_ = sessionId;
}

void FollowerPresenceService::clearWifiDirectJoinSession() {
  wifiDirectJoinSessionId_ = 0U;
}

bool FollowerPresenceService::isWifiDirectJoinSession(uint32_t sessionId) const {
  return wifiDirectJoinSessionId_ != 0U && wifiDirectJoinSessionId_ == sessionId;
}

bool FollowerPresenceService::consumeWifiDirectSessionEnd() {
  if (!pendingWifiDirectSessionEnd_) {
    return false;
  }
  pendingWifiDirectSessionEnd_ = false;
  directWifiSessionActive_ = false;
  activeWifiDirectSessionId_ = 0U;
  return true;
}

void FollowerPresenceService::tick(const char *localIp) {
  if (!started_) {
    return;
  }

  // Wi-Fi mode transitions can deinit ESP-NOW on ESP32. Keep transport alive
  // so pairing/presence never silently dies when router is unavailable.
  if (!ensureEspNowTransportReady()) {
    return;
  }

  if (localIp != nullptr && localIp[0] != '\0') {
    strncpy(lastLocalIp_, localIp, sizeof(lastLocalIp_) - 1);
    lastLocalIp_[sizeof(lastLocalIp_) - 1] = '\0';
  }

  const uint32_t nowMs = millis();
  const bool teleopActive = isTeleopTrafficActive(nowMs);
  const bool mustTransmit = forcePresenceTx_ || stagedAckPending_;

  if (directWifiSessionActive_ && !mustTransmit) {
    return;
  }

  if (!hasPairedLeader()) {
    if ((nowMs - lastPairRequestMs_) >= config::follower::kPairRequestIntervalMs) {
      lastPairRequestMs_ = nowMs;
      sendPairRequest(localIp);
      linkHeartbeat_.markOutboundSent(nowMs);
    }
    return;
  }

  if (teleopActive && !mustTransmit) {
    return;
  }

  const uint32_t fullPresenceIntervalMs =
      teleopActive ? link::kFullPresenceIntervalMs : config::follower::kIdleFullPresenceIntervalMs;
  const bool sendFullPresence =
      forcePresenceTx_ ||
      (!teleopActive && linkHeartbeat_.shouldSendFullPresence(nowMs, fullPresenceIntervalMs));
  const bool sendCompact =
      stagedAckPending_ ||
      (!sendFullPresence && linkHeartbeat_.shouldSendHeartbeat(nowMs, link::kHeartbeatIntervalMs));

  if (!sendFullPresence && !sendCompact) {
    const uint32_t pairRequestIntervalMs =
        teleopActive ? config::follower::kPairRequestIntervalTeleopMs : config::follower::kPairRequestIntervalMs;
    if ((nowMs - lastPairRequestMs_) >= pairRequestIntervalMs) {
      lastPairRequestMs_ = nowMs;
      sendPairRequest(localIp);
      linkHeartbeat_.markOutboundSent(nowMs);
    }
    return;
  }

  forcePresenceTx_ = false;

  if (sendFullPresence) {
    sendPresence(localIp);
    linkHeartbeat_.markFullPresenceSent(nowMs);
  } else {
    sendLinkHeartbeat(localIp);
    linkHeartbeat_.markOutboundSent(nowMs);
    stagedAckPending_ = false;
  }

  const uint32_t pairRequestIntervalMs =
      teleopActive ? config::follower::kPairRequestIntervalTeleopMs : config::follower::kPairRequestIntervalMs;
  if ((nowMs - lastPairRequestMs_) >= pairRequestIntervalMs) {
    lastPairRequestMs_ = nowMs;
    sendPairRequest(localIp);
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
  linkHeartbeat_.reset();
  stagedAckPending_ = false;
  Serial.println("[PAIR] NVS cleared");

  addBroadcastPeer();
  Serial.println("[PAIR] Broadcast peer re-added");

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
    uint16_t &requestId,
    bool &turbo) {
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
  turbo = batch.turbo;
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

void FollowerPresenceService::resetTeleopTransportState() {
  teleopBatchQueueHead_ = 0U;
  teleopBatchQueueTail_ = 0U;
  teleopBatchQueueCount_ = 0U;
  lastTeleopBatchRxMs_ = 0U;
  lastWifiTeleopRxMs_ = 0U;
  turboDecodeSession_.reset();
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
  stagedAckPending_ = true;
}

void FollowerPresenceService::requestImmediatePresenceTx() {
  forcePresenceTx_ = true;
}

void FollowerPresenceService::sendLinkKeepalive(const char *localIp) {
  if (!started_ || !hasPairedLeaderMac_) {
    return;
  }
  sendLinkHeartbeat(localIp);
  linkHeartbeat_.markOutboundSent(millis());
}

void FollowerPresenceService::notifyWifiTeleopActivity() {
  const uint32_t nowMs = millis();
  lastWifiTeleopRxMs_ = nowMs;
  linkHeartbeat_.notifyPeerActivity(nowMs);
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
