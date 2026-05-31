#pragma once

#include "presence_packet.h"

#include <cstdint>

namespace soarm {

// Compact ESP-NOW frame for keepalive + staged command ACK (no servo telemetry strings).
struct LinkHeartbeatPacket {
  uint8_t magic;
  uint8_t version;
  uint8_t messageType;
  uint8_t ackStatus;
  uint16_t ackRequestId;
  uint8_t ackCommandOp;
  uint8_t servoCount;
  char ip[16];
} __attribute__((packed));

static_assert(sizeof(LinkHeartbeatPacket) < sizeof(PresencePacket), "heartbeat must be smaller than presence");

} // namespace soarm
