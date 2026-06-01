#pragma once

#include "wifi_direct_offer_packet.h"

#include <cstdint>

namespace soarm {

enum class WifiDirectAckStatus : uint8_t {
  Rejected = 0,
  Connected = 1,
};

struct WifiDirectCredentials {
  uint32_t sessionId{0U};
  uint8_t channel{1U};
  char ssid[24]{};
  char psk[32]{};
};

bool generateWifiDirectCredentials(uint32_t sessionId, WifiDirectCredentials &out);
void buildWifiDirectOfferPacket(const WifiDirectCredentials &credentials, WifiDirectOfferPacket &out);
bool validateWifiDirectOfferPacket(const WifiDirectOfferPacket &packet, WifiDirectCredentials &out);
void buildWifiDirectAckPacket(
    uint32_t sessionId,
    WifiDirectAckStatus status,
    const char *followerStaIp,
    WifiDirectAckPacket &out);
bool validateWifiDirectAckPacket(const WifiDirectAckPacket &packet, uint32_t expectedSessionId);

} // namespace soarm
