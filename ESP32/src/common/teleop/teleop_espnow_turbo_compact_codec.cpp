#include "teleop_espnow_turbo_compact_codec.h"

#include "teleop_espnow_turbo_compact_packet.h"
#include "teleop_position_12bit_pack.h"

#include "../../Config/common_runtime_config.h"
#include "../presence/presence_constants.h"
#include "../presence/presence_message_type.h"

#include <cstring>

namespace soarm {

namespace {

void fillSlotsFromPayload(
    const TeleopEspNowBatchPayload &payload,
    uint8_t &activeMask,
    uint16_t slots[teleop_position_pack::kSlotCount]) {
  activeMask = 0U;
  for (uint8_t i = 0U; i < teleop_position_pack::kSlotCount; ++i) {
    slots[i] = 0U;
  }

  for (uint8_t i = 0U; i < payload.count && i < config::common::kTeleopBatchMaxServos; ++i) {
    const uint8_t id = payload.ids[i];
    if (id == 0U || id > teleop_position_pack::kSlotCount) {
      continue;
    }
    const uint8_t slot = static_cast<uint8_t>(id - 1U);
    slots[slot] = teleop_position_pack::clampStsPosition(payload.positions[i]);
    activeMask = static_cast<uint8_t>(activeMask | (1U << slot));
  }
}

void fillPayloadFromSlots(
    uint8_t activeMask,
    const uint16_t slots[teleop_position_pack::kSlotCount],
    uint16_t requestId,
    uint8_t speedPct,
    TeleopEspNowBatchPayload &payload) {
  payload.count = 0U;
  payload.requestId = requestId;
  payload.speedPct = speedPct;
  payload.turbo = true;

  for (uint8_t slot = 0U; slot < teleop_position_pack::kSlotCount; ++slot) {
    if ((activeMask & (1U << slot)) == 0U) {
      continue;
    }
    if (payload.count >= config::common::kTeleopBatchMaxServos) {
      break;
    }
    payload.ids[payload.count] = static_cast<uint8_t>(slot + 1U);
    payload.positions[payload.count] = static_cast<int16_t>(slots[slot]);
    ++payload.count;
  }
}

} // namespace

bool TeleopEspNowTurboCompactCodec::encode(
    const TeleopEspNowBatchPayload &payload,
    uint8_t *buffer,
    size_t capacity,
    size_t &outLen) const {
  if (buffer == nullptr || payload.count == 0U) {
    return false;
  }
  if (capacity < sizeof(teleop_espnow::TeleopEspNowTurboPacket)) {
    return false;
  }

  uint8_t activeMask = 0U;
  uint16_t slots[teleop_position_pack::kSlotCount]{};
  fillSlotsFromPayload(payload, activeMask, slots);
  if (activeMask == 0U) {
    return false;
  }

  teleop_espnow::TeleopEspNowTurboPacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = teleop_espnow::kTurboPacketVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::TeleopMirrorCompact);
  packet.activeMask = activeMask;
  packet.requestId = payload.requestId;
  packet.speedPct = payload.speedPct;
  teleop_position_pack::pack6Slots12Bit(slots, packet.positionsPacked);

  memcpy(buffer, &packet, sizeof(packet));
  outLen = sizeof(packet);
  return true;
}

bool TeleopEspNowTurboCompactCodec::decode(const uint8_t *buffer, size_t len, TeleopEspNowBatchPayload &payload) const {
  if (!teleop_espnow::isTeleopEspNowTurboPacket(buffer, len)) {
    return false;
  }

  teleop_espnow::TeleopEspNowTurboPacket packet{};
  memcpy(&packet, buffer, sizeof(packet));

  uint16_t slots[teleop_position_pack::kSlotCount]{};
  teleop_position_pack::unpack6Slots12Bit(packet.positionsPacked, slots);
  fillPayloadFromSlots(packet.activeMask, slots, packet.requestId, packet.speedPct, payload);
  return payload.count > 0U;
}

size_t TeleopEspNowTurboCompactCodec::encodedSize() const {
  return sizeof(teleop_espnow::TeleopEspNowTurboPacket);
}

} // namespace soarm
