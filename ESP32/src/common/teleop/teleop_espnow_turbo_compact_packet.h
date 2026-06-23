#pragma once

#include "../presence/presence_constants.h"
#include "../presence/presence_message_type.h"
#include "teleop_espnow_turbo_config.h"
#include "teleop_position_12bit_pack.h"

#include <cstddef>
#include <cstdint>

namespace soarm {
namespace teleop_espnow {

struct TeleopEspNowTurboHeaderV2 {
  uint8_t magic;
  uint8_t version;
  uint8_t messageType;
  uint8_t activeMask;
  uint16_t requestId;
  uint8_t control;
} __attribute__((packed));

static_assert(sizeof(TeleopEspNowTurboHeaderV2) == 7U, "turbo v2 header size");

constexpr size_t kTurboHeaderV2Size = sizeof(TeleopEspNowTurboHeaderV2);
constexpr size_t kTurboMaxWireSize =
    kTurboHeaderV2Size + teleop_position_pack::kPackedByteLength;

/// Packs speed (0-100) in bits 0-6 and sets the keyframe flag in bit 7.
inline uint8_t encodeControlByte(uint8_t speedPct, bool keyframe) {
  const uint8_t clampedSpeed = static_cast<uint8_t>(speedPct > 100U ? 100U : speedPct);
  return static_cast<uint8_t>((clampedSpeed & kTurboSpeedPctMask) | (keyframe ? kTurboKeyframeFlag : 0U));
}

/// Returns speed percent from the turbo control byte (bits 0-6).
inline uint8_t decodeSpeedPct(uint8_t control) {
  return static_cast<uint8_t>(control & kTurboSpeedPctMask);
}

/// True when the turbo control byte marks a keyframe (bit 7 set).
inline bool decodeKeyframeFlag(uint8_t control) {
  return (control & kTurboKeyframeFlag) != 0U;
}

/// Expected on-wire length for a turbo packet, or 0 when the header is invalid.
size_t expectedTurboWireSize(const uint8_t *data, size_t len);

/// True when len matches the turbo v2 wire size implied by activeMask.
bool isTeleopEspNowTurboPacket(const uint8_t *data, size_t len);

} // namespace teleop_espnow
} // namespace soarm
