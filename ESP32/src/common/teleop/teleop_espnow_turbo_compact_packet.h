#pragma once

#include "../presence/presence_constants.h"
#include "../presence/presence_message_type.h"
#include "teleop_position_12bit_pack.h"

#include <cstdint>

namespace soarm {
namespace teleop_espnow {

constexpr uint8_t kTurboPacketVersion = 1U;

struct TeleopEspNowTurboPacket {
  uint8_t magic;
  uint8_t version;
  uint8_t messageType;
  uint8_t activeMask;
  uint16_t requestId;
  uint8_t speedPct;
  uint8_t positionsPacked[teleop_position_pack::kPackedByteLength];
} __attribute__((packed));

static_assert(sizeof(TeleopEspNowTurboPacket) == 16U, "turbo ESP-NOW packet must stay compact");

inline bool isTeleopEspNowTurboPacket(const uint8_t *data, size_t len) {
  if (data == nullptr || len != sizeof(TeleopEspNowTurboPacket)) {
    return false;
  }
  const auto *packet = reinterpret_cast<const TeleopEspNowTurboPacket *>(data);
  return packet->magic == kPresenceMagic &&
         packet->version == kTurboPacketVersion &&
         packet->messageType == static_cast<uint8_t>(PresenceMessageType::TeleopMirrorCompact);
}

} // namespace teleop_espnow
} // namespace soarm
