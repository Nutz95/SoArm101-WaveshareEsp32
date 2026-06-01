#pragma once

#include "../presence/presence_constants.h"

#include <cstdint>

namespace soarm {

constexpr uint8_t kWifiDirectPacketVersion = 1U;
constexpr uint16_t kWifiDirectTeleopPort = 29110U;
constexpr char kWifiDirectLeaderApIp[] = "192.168.4.1";

// ESP-NOW binary offer: leader soft-AP credentials for direct Wi-Fi teleop.
struct WifiDirectOfferPacket {
  uint8_t magic;
  uint8_t version;
  uint8_t messageType;
  uint8_t channel;
  uint32_t sessionId;
  uint16_t teleopPort;
  uint16_t reserved;
  char ssid[24];
  char psk[32];
  char leaderApIp[16];
} __attribute__((packed));

// ESP-NOW binary ack: follower STA joined the leader AP.
struct WifiDirectAckPacket {
  uint8_t magic;
  uint8_t version;
  uint8_t messageType;
  uint8_t status;
  uint32_t sessionId;
  char followerStaIp[16];
} __attribute__((packed));


} // namespace soarm
