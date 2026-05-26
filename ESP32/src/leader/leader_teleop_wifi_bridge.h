#pragma once

#include "../common/teleop/teleop_wifi_packet.h"

#include <WiFiUdp.h>
#include <cstdint>

namespace soarm {

class LeaderTeleopWifiBridge {
public:
  bool begin(uint16_t localPort);
  bool sendBatch(
      const char *followerIp,
      const uint8_t *ids,
      const int16_t *positions,
      uint8_t count,
      uint8_t speedPercent,
      uint16_t requestId);
  bool pollAck(uint16_t &requestId, uint8_t &status);

private:
  WiFiUDP udp_;
  bool started_{false};
};

} // namespace soarm
