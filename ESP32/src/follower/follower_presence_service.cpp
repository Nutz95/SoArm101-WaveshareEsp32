#include "follower_presence_service.h"

#include "../common/presence_protocol.h"

#include <Arduino.h>
#include <esp_now.h>
#include <cstring>

namespace soarm {

namespace {

FollowerPresenceService *gInstance = nullptr;

} // namespace

FollowerPresenceService::FollowerPresenceService()
    : pairingStore_("soarm-pair", "leader_mac") {
}

bool FollowerPresenceService::begin() {
  if (esp_now_init() != ESP_OK) {
    return false;
  }

  hasPairedLeaderMac_ = pairingStore_.load(pairedLeaderMac_);

  gInstance = this;
  esp_now_register_recv_cb(onDataRecvStatic);

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

void FollowerPresenceService::onDataRecvStatic(const uint8_t *mac, const uint8_t *data, int len) {
  if (gInstance != nullptr) {
    gInstance->onDataRecv(mac, data, len);
  }
}

void FollowerPresenceService::onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
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
  if (esp_now_is_peer_exist(mac)) {
    return true;
  }

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
  peer.channel = 0;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
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

} // namespace soarm
