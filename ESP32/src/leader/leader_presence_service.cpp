#include "leader_presence_service.h"

#include "../common/presence_protocol.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <cstring>

namespace soarm {

namespace {

LeaderPresenceService *gInstance = nullptr;

} // namespace

LeaderPresenceService::LeaderPresenceService()
    : pairingStore_("soarm-pair", "follower_mac") {
}

bool LeaderPresenceService::begin() {
  if (esp_now_init() != ESP_OK) {
    return false;
  }

  hasPairedMac_ = pairingStore_.load(pairedFollowerMac_);

  gInstance = this;
  esp_now_register_recv_cb(onDataRecvStatic);
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

void LeaderPresenceService::onDataRecvStatic(const uint8_t *mac, const uint8_t *data, int len) {
  if (gInstance != nullptr) {
    gInstance->onDataRecv(mac, data, len);
  }
}

void LeaderPresenceService::onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
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
    }

    if (isPairedMac(mac)) {
      addPeer(mac);
      sendPairAck(mac);
    }
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
  lastFollowerSeenMs_ = millis();
}

bool LeaderPresenceService::addPeer(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) {
    return true;
  }

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
  peer.channel = 0;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
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

bool LeaderPresenceService::isPairedMac(const uint8_t mac[6]) const {
  if (!hasPairedMac_) {
    return false;
  }
  return memcmp(pairedFollowerMac_, mac, sizeof(pairedFollowerMac_)) == 0;
}

} // namespace soarm
