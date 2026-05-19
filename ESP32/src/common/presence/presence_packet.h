#pragma once

#include <cstdint>

namespace soarm {

struct PresencePacket {
  uint8_t magic;
  uint8_t version;
  uint8_t messageType;
  uint8_t reserved;
  uint8_t controlOp;
  uint8_t servoCount;
  uint16_t reserved2;
  uint32_t controlValue;
  char ip[16];
  char servoIds[48];
  char servoTelemetry[96];
} __attribute__((packed));

} // namespace soarm
