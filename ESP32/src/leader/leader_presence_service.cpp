#include "leader_presence_service.h"

#include "../common/presence/presence_constants.h"
#include "../common/presence/presence_message_type.h"
#include "../common/presence/presence_packet.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <cstdio>
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
    formatMac(pairedFollowerMac_, pairedFollowerMacText_);
  } else {
    strncpy(pairedFollowerMacText_, "unpaired", sizeof(pairedFollowerMacText_) - 1);
    pairedFollowerMacText_[sizeof(pairedFollowerMacText_) - 1] = '\0';
  }

  const String localMac = WiFi.macAddress();
  strncpy(localMacText_, localMac.c_str(), sizeof(localMacText_) - 1);
  localMacText_[sizeof(localMacText_) - 1] = '\0';

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

bool LeaderPresenceService::resetPairing() {
  hasPairedMac_ = false;
  memset(pairedFollowerMac_, 0, sizeof(pairedFollowerMac_));
  strncpy(pairedFollowerMacText_, "unpaired", sizeof(pairedFollowerMacText_) - 1);
  pairedFollowerMacText_[sizeof(pairedFollowerMacText_) - 1] = '\0';
  followerIp_[0] = '\0';
  lastFollowerSeenMs_ = 0;
  pairingStore_.clear();
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
        formatMac(pairedFollowerMac_, pairedFollowerMacText_);
      }
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

void LeaderPresenceService::formatMac(const uint8_t mac[6], char out[18]) const {
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
