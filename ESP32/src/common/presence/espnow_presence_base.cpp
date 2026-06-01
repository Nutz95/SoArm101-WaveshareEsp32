#include "espnow_presence_base.h"

#include <esp_now.h>
#include <cstring>

namespace soarm {

EspNowPresenceBase *EspNowPresenceBase::activeInstance_ = nullptr;

bool EspNowPresenceBase::initEspNow() {
  if (esp_now_init() != ESP_OK) {
    return false;
  }

  activeInstance_ = this;
  esp_now_register_recv_cb(onDataRecvStatic);
  return true;
}

bool EspNowPresenceBase::ensureEspNowReady() {
  const esp_err_t err = esp_now_init();
  if (err == ESP_OK) {
    activeInstance_ = this;
    esp_now_register_recv_cb(onDataRecvStatic);
    return true;
  }
  if (err == ESP_ERR_ESPNOW_EXIST) {
    return true;
  }
  return false;
}

bool EspNowPresenceBase::addPeer(const uint8_t mac[6], uint8_t channel) {
  if (esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t existing{};
    if (esp_now_get_peer(mac, &existing) != ESP_OK) {
      esp_now_del_peer(mac);
    } else if (channel == 0U) {
      // Channel 0 means "follow current channel". If peer was pinned to a fixed channel
      // during Wi-Fi direct, recreate it as dynamic to avoid later channel-mismatch sends.
      if (existing.channel == 0U) {
        return true;
      }
      esp_now_del_peer(mac);
    } else if (existing.channel != channel) {
      esp_now_del_peer(mac);
    } else {
      return true;
    }
  }

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
  peer.channel = channel;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void EspNowPresenceBase::onDataRecvStatic(const uint8_t *mac, const uint8_t *data, int len) {
  if (activeInstance_ != nullptr) {
    activeInstance_->onPresenceFrame(mac, data, len);
  }
}

} // namespace soarm
