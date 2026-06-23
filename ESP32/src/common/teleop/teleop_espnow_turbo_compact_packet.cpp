#include "teleop_espnow_turbo_compact_packet.h"

namespace soarm {
namespace teleop_espnow {

size_t expectedTurboWireSize(const uint8_t *data, size_t len) {
  if (data == nullptr || len < 4U) {
    return 0U;
  }
  if (data[0] != kPresenceMagic ||
      data[2] != static_cast<uint8_t>(PresenceMessageType::TeleopMirrorCompact)) {
    return 0U;
  }

  if (data[1] != kTurboPacketVersionV2) {
    return 0U;
  }

  if (len < kTurboHeaderV2Size) {
    return 0U;
  }
  const uint8_t activeMask = data[3];
  return kTurboHeaderV2Size + teleop_position_pack::packedByteLengthForMask(activeMask);
}

bool isTeleopEspNowTurboPacket(const uint8_t *data, size_t len) {
  const size_t expected = expectedTurboWireSize(data, len);
  return expected != 0U && len == expected;
}

} // namespace teleop_espnow
} // namespace soarm
