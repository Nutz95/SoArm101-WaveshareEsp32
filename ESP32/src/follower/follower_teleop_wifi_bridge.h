#pragma once

#include "../common/teleop/teleop_wifi_packet.h"

#include <WiFiUdp.h>
#include <cstdint>

namespace soarm {

class FollowerTeleopWifiBridge {
public:
  bool begin(uint16_t localPort);
  bool consumeBatch(
      uint8_t *ids,
      int16_t *positions,
      uint8_t capacity,
      uint8_t &count,
      uint8_t &speedPercent,
      uint16_t &requestId);
  bool sendAck(uint16_t requestId, uint8_t status);

private:
  WiFiUDP udp_;
  IPAddress leaderAckIp_{};
  uint16_t leaderAckPort_{0U};
  bool hasAckPeer_{false};
  bool started_{false};
};

} // namespace soarm
