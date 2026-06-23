#include "leader_presence_service.h"

#include "../common/presence/presence_constants.h"
#include "../common/presence/presence_message_type.h"
#include "../common/presence/presence_packet.h"
#include "../common/teleop/teleop_espnow_batch_payload.h"
#include "../common/teleop/teleop_espnow_legacy_batch_codec.h"
#include "../common/teleop/teleop_espnow_turbo_compact_codec.h"
#include "../common/wifi/wifi_direct_offer_packet.h"
#include "../common/wifi/wifi_direct_session.h"
#include "../common/servo/servo_control_opcode.h"
#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <cstring>

namespace soarm {

void LeaderPresenceService::sendPairAck(const uint8_t mac[6]) {
  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::PairAck);
  strncpy(packet.ip, WiFi.localIP().toString().c_str(), sizeof(packet.ip) - 1);
  packet.ip[sizeof(packet.ip) - 1] = '\0';

  esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

void LeaderPresenceService::sendPairResetTo(const uint8_t mac[6]) {
  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::PairReset);
  strncpy(packet.ip, WiFi.localIP().toString().c_str(), sizeof(packet.ip) - 1);
  packet.ip[sizeof(packet.ip) - 1] = '\0';

  addPeer(mac);

  Serial.printf("[PAIR] Sending PairReset to %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
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

void LeaderPresenceService::sendServoScanBroadcast(uint16_t requestId) {
  sendServoControlBroadcast(static_cast<uint8_t>(ServoControlOpcode::Scan), 0U, requestId);
}

bool LeaderPresenceService::sendServoControl(const uint8_t mac[6], uint8_t op, uint32_t value, uint16_t requestId) {
  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::ServoControl);
  packet.reserved = static_cast<uint8_t>(requestId & 0xFFU);
  packet.controlOp = op;
  packet.reserved2 = requestId;
  packet.controlValue = value;
  strncpy(packet.ip, WiFi.localIP().toString().c_str(), sizeof(packet.ip) - 1);
  packet.ip[sizeof(packet.ip) - 1] = '\0';

  const uint8_t sendAttempts = (op == static_cast<uint8_t>(ServoControlOpcode::TeleopMirror))
                                   ? 1U
                                   : config::leader::kFollowerInitialSendBurstCount;

  bool sent = false;
  for (uint8_t attempt = 0U; attempt < sendAttempts; ++attempt) {
    if (esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet)) == ESP_OK) {
      sent = true;
    }
  }
  return sent;
}

bool LeaderPresenceService::sendServoControlBatch(
    const uint8_t mac[6],
    const uint8_t *ids,
    const int16_t *positions,
    uint8_t count,
    uint8_t speedPct,
    uint16_t requestId,
    bool turbo) {
  if (ids == nullptr || positions == nullptr || count == 0U) {
    return false;
  }

  const uint8_t clampedCount = (count > config::common::kTeleopBatchMaxServos)
                                   ? config::common::kTeleopBatchMaxServos
                                   : count;

  TeleopEspNowBatchPayload payload{};
  payload.count = clampedCount;
  payload.speedPct = speedPct;
  payload.requestId = requestId;
  payload.turbo = turbo;
  for (uint8_t i = 0U; i < clampedCount; ++i) {
    payload.ids[i] = ids[i];
    payload.positions[i] = positions[i];
  }

  static const TeleopEspNowLegacyBatchCodec kLegacyCodec;
  static const TeleopEspNowTurboCompactCodec kTurboCodec;
  const ITeleopEspNowBatchCodec &codec = turbo ? static_cast<const ITeleopEspNowBatchCodec &>(kTurboCodec)
                                               : static_cast<const ITeleopEspNowBatchCodec &>(kLegacyCodec);

  uint8_t buffer[sizeof(PresencePacket)]{};
  size_t outLen = 0U;
  if (!codec.encode(payload, buffer, sizeof(buffer), outLen)) {
    return false;
  }

  if (!ensureEspNowTransportReady()) {
    return false;
  }

  return esp_now_send(mac, buffer, outLen) == ESP_OK;
}

bool LeaderPresenceService::sendServoControlBroadcast(uint8_t op, uint32_t value, uint16_t requestId) {
  uint8_t broadcastAddr[ESP_NOW_ETH_ALEN];
  memset(broadcastAddr, 0xFF, sizeof(broadcastAddr));
  if (!addPeer(broadcastAddr)) {
    return false;
  }
  return sendServoControl(broadcastAddr, op, value, requestId);
}

bool LeaderPresenceService::addBroadcastPeer() {
  uint8_t broadcastAddr[ESP_NOW_ETH_ALEN];
  memset(broadcastAddr, 0xFF, sizeof(broadcastAddr));
  return addPeer(broadcastAddr);
}

bool LeaderPresenceService::ensureEspNowTransportReady(uint8_t wifiChannel) {
  if (!started_) {
    return false;
  }
  if (!ensureEspNowReady()) {
    return false;
  }
  if (wifiChannel != 0U) {
    esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE);
  }
  if (!addBroadcastPeer()) {
    return false;
  }
  if (hasPairedMac_ && !addPeer(pairedFollowerMac_, wifiChannel)) {
    return false;
  }
  return true;
}

bool LeaderPresenceService::sendWifiDirectOffer(const WifiDirectOfferPacket &packet) {
  if (!hasPairedMac_) {
    return false;
  }

  if (!ensureEspNowTransportReady(packet.channel == 0U ? 1U : packet.channel)) {
    return false;
  }

  followerWifiDirectSessionId_ = packet.sessionId;
  followerWifiDirectIp_[0] = '\0';
  const esp_err_t result =
      esp_now_send(pairedFollowerMac_, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  return result == ESP_OK;
}

bool LeaderPresenceService::sendWifiDirectSessionEnd() {
  if (!hasPairedMac_ || followerWifiDirectSessionId_ == 0U) {
    return false;
  }

  if (!ensureEspNowTransportReady()) {
    return false;
  }

  WifiDirectAckPacket packet{};
  buildWifiDirectAckPacket(
      followerWifiDirectSessionId_, WifiDirectAckStatus::SessionEnd, nullptr, packet);
  const esp_err_t result =
      esp_now_send(pairedFollowerMac_, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  return result == ESP_OK;
}

bool LeaderPresenceService::isPairedMac(const uint8_t mac[6]) const {
  if (!hasPairedMac_) {
    return false;
  }
  return memcmp(pairedFollowerMac_, mac, sizeof(pairedFollowerMac_)) == 0;
}

} // namespace soarm
