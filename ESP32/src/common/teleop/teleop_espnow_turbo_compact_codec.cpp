#include "teleop_espnow_turbo_compact_codec.h"

#include "teleop_espnow_turbo_compact_packet.h"
#include "teleop_espnow_turbo_keyframe_policy.h"
#include "teleop_position_12bit_pack.h"

#include "../../Config/common_runtime_config.h"
#include "../presence/presence_constants.h"
#include "../presence/presence_message_type.h"

#include <cstring>

namespace soarm {

namespace {

bool decodeTurboV2(
    const uint8_t *buffer,
    size_t len,
    TeleopEspNowTurboSession &session,
    TeleopEspNowBatchPayload &payload) {
  if (len < teleop_espnow::kTurboHeaderV2Size) {
    return false;
  }

  teleop_espnow::TeleopEspNowTurboHeaderV2 header{};
  memcpy(&header, buffer, sizeof(header));
  const uint8_t speedPct = teleop_espnow::decodeSpeedPct(header.control);
  const bool keyframe = teleop_espnow::decodeKeyframeFlag(header.control);

  const uint8_t *positionBytes = buffer + teleop_espnow::kTurboHeaderV2Size;
  const size_t positionLen = len - teleop_espnow::kTurboHeaderV2Size;

  uint16_t slots[teleop_position_pack::kSlotCount]{};
  if (!teleop_position_pack::unpackMaskedSlots12Bit(
          header.activeMask, positionBytes, static_cast<uint8_t>(positionLen), slots)) {
    return false;
  }

  if (keyframe) {
    session.knownMask = header.activeMask;
  } else {
    session.knownMask = static_cast<uint8_t>(session.knownMask | header.activeMask);
  }

  for (uint8_t slot = 0U; slot < teleop_position_pack::kSlotCount; ++slot) {
    if ((header.activeMask & (1U << slot)) == 0U) {
      continue;
    }
    session.slots[slot] = slots[slot];
  }

  session.buildApplyPayload(header.activeMask, header.requestId, speedPct, payload);
  return payload.count > 0U;
}

} // namespace

bool TeleopEspNowTurboCompactCodec::encodeWithSession(
    const TeleopEspNowBatchPayload &payload,
    TeleopEspNowTurboSession &session,
    uint32_t nowMs,
    uint8_t *buffer,
    size_t capacity,
    size_t &outLen) const {
  if (buffer == nullptr || payload.count == 0U) {
    return false;
  }
  if (capacity < teleop_espnow::kTurboMaxWireSize) {
    return false;
  }

  session.mergePayload(payload);

  const bool keyframe = TeleopEspNowTurboKeyframePolicy::shouldSendKeyframe(session, nowMs);
  const uint8_t deltaMask = session.maskFromPayload(payload);
  const uint8_t activeMask = keyframe ? session.knownMask : deltaMask;
  if (activeMask == 0U) {
    return false;
  }

  teleop_espnow::TeleopEspNowTurboHeaderV2 header{};
  header.magic = kPresenceMagic;
  header.version = teleop_espnow::kTurboPacketVersionV2;
  header.messageType = static_cast<uint8_t>(PresenceMessageType::TeleopMirrorCompact);
  header.activeMask = activeMask;
  header.requestId = payload.requestId;
  header.control = teleop_espnow::encodeControlByte(payload.speedPct, keyframe);

  uint8_t *positionBytes = buffer + teleop_espnow::kTurboHeaderV2Size;
  const uint8_t positionCapacity =
      static_cast<uint8_t>(capacity - teleop_espnow::kTurboHeaderV2Size);
  teleop_position_pack::packMaskedSlots12Bit(activeMask, session.slots, positionBytes, positionCapacity);

  memcpy(buffer, &header, sizeof(header));
  outLen = teleop_espnow::kTurboHeaderV2Size + teleop_position_pack::packedByteLengthForMask(activeMask);
  session.noteFrameSent(keyframe, nowMs);
  return true;
}

bool TeleopEspNowTurboCompactCodec::decodeWithSession(
    const uint8_t *buffer,
    size_t len,
    TeleopEspNowTurboSession &session,
    TeleopEspNowBatchPayload &payload) const {
  if (!teleop_espnow::isTeleopEspNowTurboPacket(buffer, len)) {
    return false;
  }

  if (buffer[1] != teleop_espnow::kTurboPacketVersionV2) {
    return false;
  }

  return decodeTurboV2(buffer, len, session, payload);
}

bool TeleopEspNowTurboCompactCodec::encode(
    const TeleopEspNowBatchPayload &payload,
    uint8_t *buffer,
    size_t capacity,
    size_t &outLen) const {
  TeleopEspNowTurboSession session{};
  return encodeWithSession(payload, session, 0U, buffer, capacity, outLen);
}

bool TeleopEspNowTurboCompactCodec::decode(const uint8_t *buffer, size_t len, TeleopEspNowBatchPayload &payload) const {
  TeleopEspNowTurboSession session{};
  return decodeWithSession(buffer, len, session, payload);
}

size_t TeleopEspNowTurboCompactCodec::encodedSize() const {
  return teleop_espnow::kTurboMaxWireSize;
}

} // namespace soarm
