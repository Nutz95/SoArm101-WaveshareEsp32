#include "leader_presence_service.h"

#include "../common/presence/presence_constants.h"
#include "../common/presence/presence_message_type.h"
#include "../common/presence/presence_packet.h"

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

bool LeaderPresenceService::resetPairing() {
  uint8_t previousPairMac[6]{};
  const bool wasPaired = hasPairedMac_;
  if (wasPaired) {
    memcpy(previousPairMac, pairedFollowerMac_, sizeof(previousPairMac));
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

  if (wasPaired) {
    PresencePacket packet{};
    packet.magic = kPresenceMagic;
    packet.version = kPresenceVersion;
    packet.messageType = static_cast<uint8_t>(PresenceMessageType::PairReset);
    strncpy(packet.ip, WiFi.localIP().toString().c_str(), sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
    esp_now_send(previousPairMac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  }

  sendPairResetBroadcast();
  return true;
}

bool LeaderPresenceService::requestServoScan() {
  sendServoScanBroadcast();
  return true;
}

bool LeaderPresenceService::requestServoControl(uint8_t op, uint32_t value) {
  if (hasPairedMac_) {
    sendServoControl(pairedFollowerMac_, op, value);
  } else {
    sendServoControlBroadcast(op, value);
  }
  return true;
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

  const PresenceMessageType msgType = static_cast<PresenceMessageType>(packet.messageType);

  if (msgType == PresenceMessageType::PairRequest) {
    if (!hasPairedMac_) {
      memcpy(pairedFollowerMac_, mac, sizeof(pairedFollowerMac_));
      hasPairedMac_ = pairingStore_.save(pairedFollowerMac_);
      if (hasPairedMac_) {
        formatMacAddress(pairedFollowerMac_, pairedFollowerMacText_);
      }
    }

    if (isPairedMac(mac)) {
      addPeer(mac);
      sendPairAck(mac);
    }
    return;
  }

  if (msgType == PresenceMessageType::ServoScan) {
    return;
  }

  if (msgType == PresenceMessageType::ServoControl) {
    return;
  }

  if (msgType != PresenceMessageType::Presence) {
    return;
  }

  if (!isPairedMac(mac)) {
    return;
  }

  strncpy(followerIp_, packet.ip, sizeof(followerIp_) - 1);
  followerIp_[sizeof(followerIp_) - 1] = '\0';
  followerServoCount_ = packet.servoCount;
  followerServoDebugManual_ = packet.controlOp != 0U;
  strncpy(followerServoIdsText_, packet.servoIds, sizeof(followerServoIdsText_) - 1);
  followerServoIdsText_[sizeof(followerServoIdsText_) - 1] = '\0';
  strncpy(followerServoTelemetryText_, packet.servoTelemetry, sizeof(followerServoTelemetryText_) - 1);
  followerServoTelemetryText_[sizeof(followerServoTelemetryText_) - 1] = '\0';
  lastFollowerSeenMs_ = millis();
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

void LeaderPresenceService::sendServoScanBroadcast() {
  sendServoControlBroadcast(static_cast<uint8_t>(ServoControlOpcode::Scan), 0U);
}

void LeaderPresenceService::sendServoControl(const uint8_t mac[6], uint8_t op, uint32_t value) {
  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::ServoControl);
  packet.controlOp = op;
  packet.controlValue = value;
  strncpy(packet.ip, WiFi.localIP().toString().c_str(), sizeof(packet.ip) - 1);
  packet.ip[sizeof(packet.ip) - 1] = '\0';
  esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

void LeaderPresenceService::sendServoControlBroadcast(uint8_t op, uint32_t value) {
  uint8_t broadcastAddr[ESP_NOW_ETH_ALEN];
  memset(broadcastAddr, 0xFF, sizeof(broadcastAddr));
  addPeer(broadcastAddr);
  sendServoControl(broadcastAddr, op, value);
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
