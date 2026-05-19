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

bool EspNowPresenceBase::addPeer(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) {
    return true;
  }

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
  peer.channel = 0;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void EspNowPresenceBase::onDataRecvStatic(const uint8_t *mac, const uint8_t *data, int len) {
  if (activeInstance_ != nullptr) {
    activeInstance_->onPresenceFrame(mac, data, len);
  }
}

} // namespace soarm
