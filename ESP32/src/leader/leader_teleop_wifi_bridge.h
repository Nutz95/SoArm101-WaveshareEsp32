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
      uint16_t requestId,
      uint8_t flags);
  bool pollAck(uint16_t &requestId, uint8_t &status);

private:
  void logSendErrorThrottled(
      uint32_t nowMs,
      int errorCode,
      uint16_t requestId,
      uint8_t count,
      const char *ip,
      const char *stage);

  WiFiUDP udp_;
  bool started_{false};
  uint32_t lastSendErrorLogMs_{0U};
  uint32_t sendBackoffUntilMs_{0U};
};

} // namespace soarm
