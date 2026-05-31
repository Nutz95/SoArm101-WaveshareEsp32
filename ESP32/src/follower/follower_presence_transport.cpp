#include "follower_presence_service.h"

#include "../common/presence/link_heartbeat_packet.h"
#include "../common/presence/presence_constants.h"
#include "../common/presence/presence_message_type.h"
#include "../common/presence/presence_packet.h"
#include "../Config/follower_runtime_config.h"

#include <esp_now.h>
#include <cstring>

namespace soarm {

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

void FollowerPresenceService::sendCommandAck(uint16_t requestId, uint8_t op, uint8_t status, uint8_t sequence) {
  if (!hasPairedLeaderMac_) {
    return;
  }

  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::ServoCommandAck);
  packet.reserved = status;
  packet.controlOp = op;
  packet.reserved2 = requestId;
  packet.controlValue = 0U;
  if (servoDebugManual_) {
    packet.controlValue |= (1UL << 8U);
  }
  if (servoTemperatureAlarm_) {
    packet.controlValue |= (1UL << 9U);
  }
  packet.controlValue |= static_cast<uint32_t>(sequence);

  for (uint8_t attempt = 0U; attempt < config::follower::kCommandAckSendBurstCount; ++attempt) {
    esp_now_send(pairedLeaderMac_, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  }
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

  if (hasPairedLeaderMac_) {
    esp_now_send(pairedLeaderMac_, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    return;
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
  } else if (lastLocalIp_[0] != '\0') {
    strncpy(packet.ip, lastLocalIp_, sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  } else {
    strncpy(packet.ip, "0.0.0.0", sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  }

  packet.servoCount = servoCount_;
  packet.reserved = lastAckStatus_;
  packet.reserved2 = lastAckRequestId_;
  packet.controlOp = servoDebugManual_ ? 1U : 0U;
  packet.controlValue = static_cast<uint32_t>(lastAckCommandOp_);
  if (servoDebugManual_) {
    packet.controlValue |= (1UL << 8U);
  }
  if (servoTemperatureAlarm_) {
    packet.controlValue |= (1UL << 9U);
  }
  strncpy(packet.servoIds, servoIdsText_, sizeof(packet.servoIds) - 1);
  packet.servoIds[sizeof(packet.servoIds) - 1] = '\0';
  strncpy(packet.servoTelemetry, servoTelemetryText_, sizeof(packet.servoTelemetry) - 1);
  packet.servoTelemetry[sizeof(packet.servoTelemetry) - 1] = '\0';

  esp_now_send(pairedLeaderMac_, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

void FollowerPresenceService::sendLinkHeartbeat(const char *localIp) {
  LinkHeartbeatPacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::LinkHeartbeat);
  packet.ackStatus = lastAckStatus_;
  packet.ackRequestId = lastAckRequestId_;
  packet.ackCommandOp = lastAckCommandOp_;
  packet.servoCount = servoCount_;

  if (localIp != nullptr && localIp[0] != '\0') {
    strncpy(packet.ip, localIp, sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  } else if (lastLocalIp_[0] != '\0') {
    strncpy(packet.ip, lastLocalIp_, sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  } else {
    strncpy(packet.ip, "0.0.0.0", sizeof(packet.ip) - 1);
    packet.ip[sizeof(packet.ip) - 1] = '\0';
  }

  esp_now_send(pairedLeaderMac_, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

} // namespace soarm
