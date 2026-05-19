#include "follower_presence_service.h"

#include "../common/presence/presence_constants.h"
#include "../common/presence/presence_message_type.h"
#include "../common/presence/presence_packet.h"

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <cstdio>
#include <cstring>

namespace soarm {

FollowerPresenceService::FollowerPresenceService()
    : pairingStore_("soarm-pair", "leader_mac") {
}

bool FollowerPresenceService::begin() {
  if (!initEspNow()) {
    return false;
  }

  hasPairedLeaderMac_ = pairingStore_.load(pairedLeaderMac_);
  if (hasPairedLeaderMac_) {
    formatMac(pairedLeaderMac_, pairedLeaderMacText_);
  } else {
    strncpy(pairedLeaderMacText_, "unpaired", sizeof(pairedLeaderMacText_) - 1);
    pairedLeaderMacText_[sizeof(pairedLeaderMacText_) - 1] = '\0';
  }

  const String localMac = WiFi.macAddress();
  strncpy(localMacText_, localMac.c_str(), sizeof(localMacText_) - 1);
  localMacText_[sizeof(localMacText_) - 1] = '\0';

  if (!hasPairedLeaderMac_) {
    if (!addBroadcastPeer()) {
      return false;
    }
  } else {
    if (!addPeer(pairedLeaderMac_)) {
      return false;
    }
  }

  started_ = true;
  return true;
}

void FollowerPresenceService::tick(const char *localIp) {
  if (!started_) {
    return;
  }

  const uint32_t nowMs = millis();
  if ((nowMs - lastTxMs_) < kPresenceTxPeriodMs) {
    return;
  }

  lastTxMs_ = nowMs;
  if (hasPairedLeader()) {
    sendPresence(localIp);
  } else {
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
  hasPairedLeaderMac_ = false;
  memset(pairedLeaderMac_, 0, sizeof(pairedLeaderMac_));
  strncpy(pairedLeaderMacText_, "unpaired", sizeof(pairedLeaderMacText_) - 1);
  pairedLeaderMacText_[sizeof(pairedLeaderMacText_) - 1] = '\0';
  pairingStore_.clear();
  addBroadcastPeer();
  return true;
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
  if (msgType != PresenceMessageType::PairAck) {
    return;
  }

  if (!hasPairedLeaderMac_) {
    memcpy(pairedLeaderMac_, mac, sizeof(pairedLeaderMac_));
    hasPairedLeaderMac_ = pairingStore_.save(pairedLeaderMac_);
    if (hasPairedLeaderMac_) {
      formatMac(pairedLeaderMac_, pairedLeaderMacText_);
      addPeer(pairedLeaderMac_);
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
  } else {
    strncpy(packet.ip, "0.0.0.0", sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  }

  esp_now_send(pairedLeaderMac_, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

void FollowerPresenceService::formatMac(const uint8_t mac[6], char out[18]) const {
  snprintf(
      out,
      18,
      "%02X:%02X:%02X:%02X:%02X:%02X",
      mac[0],
      mac[1],
      mac[2],
      mac[3],
      mac[4],
      mac[5]);
}

} // namespace soarm
