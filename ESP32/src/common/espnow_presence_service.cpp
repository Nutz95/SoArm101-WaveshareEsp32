#include "espnow_presence_service.h"

#include <WiFi.h>
#include <esp_now.h>
#include <cstring>

namespace soarm {

namespace {

constexpr uint8_t kMsgMagic = 0xA5;
constexpr uint8_t kMsgVersion = 1;
constexpr uint32_t kFollowerTxPeriodMs = 1000U;
constexpr uint32_t kFollowerTimeoutMs = 3500U;

struct PresenceMessage {
  uint8_t magic;
  uint8_t version;
  uint8_t role;
  char ip[16];
} __attribute__((packed));

EspNowPresenceService *gInstance = nullptr;

} // namespace

bool EspNowPresenceService::begin(Role role, FollowerIpCallback onFollowerIp) {
  role_ = role;
  onFollowerIp_ = onFollowerIp;

  if (esp_now_init() != ESP_OK) {
    return false;
  }

  gInstance = this;
  esp_now_register_recv_cb(onDataRecvStatic);

  if (role_ == Role::Follower) {
    if (!addBroadcastPeer()) {
      return false;
    }
  }

  started_ = true;
  return true;
}

void EspNowPresenceService::tick(const char *localIp) {
  if (!started_) {
    return;
  }

  if (role_ == Role::Follower) {
    const uint32_t nowMs = millis();
    if ((nowMs - lastTxMs_) >= kFollowerTxPeriodMs) {
      lastTxMs_ = nowMs;
      sendFollowerPresence(localIp);
    }
  }
}

bool EspNowPresenceService::isFollowerLinked() const {
  if (role_ != Role::Leader) {
    return false;
  }
  const uint32_t nowMs = millis();
  return ((nowMs - lastFollowerSeenMs_) <= kFollowerTimeoutMs) && hasValidFollowerIp();
}

bool EspNowPresenceService::hasValidFollowerIp() const {
  if (followerIp_[0] == '\0') {
    return false;
  }
  return strcmp(followerIp_, "0.0.0.0") != 0;
}

const char *EspNowPresenceService::followerIp() const {
  return followerIp_;
}

void EspNowPresenceService::onDataRecvStatic(const uint8_t * /*mac*/, const uint8_t *data, int len) {
  if (gInstance) {
    gInstance->handleReceived(data, len);
  }
}

void EspNowPresenceService::handleReceived(const uint8_t *data, int len) {
  if (role_ != Role::Leader) {
    return;
  }

  if (len != static_cast<int>(sizeof(PresenceMessage))) {
    return;
  }

  PresenceMessage msg{};
  memcpy(&msg, data, sizeof(msg));

  if (msg.magic != kMsgMagic || msg.version != kMsgVersion) {
    return;
  }

  if (msg.role != static_cast<uint8_t>(Role::Follower)) {
    return;
  }

  strncpy(followerIp_, msg.ip, sizeof(followerIp_) - 1);
  followerIp_[sizeof(followerIp_) - 1] = '\0';
  lastFollowerSeenMs_ = millis();

  if (onFollowerIp_) {
    onFollowerIp_(followerIp_);
  }
}

bool EspNowPresenceService::addBroadcastPeer() {
  esp_now_peer_info_t peer{};
  memset(peer.peer_addr, 0xFF, ESP_NOW_ETH_ALEN);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_is_peer_exist(peer.peer_addr)) {
    return true;
  }

  return esp_now_add_peer(&peer) == ESP_OK;
}

void EspNowPresenceService::sendFollowerPresence(const char *localIp) {
  PresenceMessage msg{};
  msg.magic = kMsgMagic;
  msg.version = kMsgVersion;
  msg.role = static_cast<uint8_t>(Role::Follower);

  if (localIp && localIp[0] != '\0') {
    strncpy(msg.ip, localIp, sizeof(msg.ip) - 1);
    msg.ip[sizeof(msg.ip) - 1] = '\0';
  } else {
    strncpy(msg.ip, "0.0.0.0", sizeof(msg.ip) - 1);
    msg.ip[sizeof(msg.ip) - 1] = '\0';
  }

  uint8_t broadcastAddr[ESP_NOW_ETH_ALEN];
  memset(broadcastAddr, 0xFF, sizeof(broadcastAddr));
  esp_now_send(broadcastAddr, reinterpret_cast<const uint8_t *>(&msg), sizeof(msg));
}

} // namespace soarm
