#include "teleop_espnow_legacy_batch_codec.h"

#include "../../Config/common_runtime_config.h"
#include "../presence/presence_constants.h"
#include "../presence/presence_message_type.h"
#include "../presence/presence_packet.h"
#include "../servo/servo_control_opcode.h"

#include <cstring>

namespace soarm {

namespace {

bool decodeLegacyBatchFromPresence(const PresencePacket &packet, TeleopEspNowBatchPayload &payload) {
  if (packet.controlOp != static_cast<uint8_t>(ServoControlOpcode::TeleopMirrorBatch)) {
    return false;
  }

  const uint8_t rawCount = static_cast<uint8_t>(packet.servoTelemetry[0]);
  const uint8_t clampedCount = (rawCount > config::common::kTeleopBatchMaxServos)
                                   ? config::common::kTeleopBatchMaxServos
                                   : rawCount;
  payload.count = clampedCount;
  payload.speedPct = static_cast<uint8_t>(packet.servoTelemetry[1] & 0x7FU);
  payload.requestId = packet.reserved2;
  payload.turbo = (static_cast<uint8_t>(packet.servoTelemetry[1]) & 0x80U) != 0U;

  for (uint8_t i = 0U; i < clampedCount; ++i) {
    const uint8_t offset = static_cast<uint8_t>(2U + (i * 3U));
    payload.ids[i] = static_cast<uint8_t>(packet.servoTelemetry[offset]);
    const uint16_t lo = static_cast<uint8_t>(packet.servoTelemetry[offset + 1U]);
    const uint16_t hi = static_cast<uint8_t>(packet.servoTelemetry[offset + 2U]);
    payload.positions[i] = static_cast<int16_t>((hi << 8U) | lo);
  }
  return true;
}

} // namespace

bool TeleopEspNowLegacyBatchCodec::encode(
    const TeleopEspNowBatchPayload &payload,
    uint8_t *buffer,
    size_t capacity,
    size_t &outLen) const {
  if (buffer == nullptr || payload.count == 0U) {
    return false;
  }
  if (capacity < sizeof(PresencePacket)) {
    return false;
  }

  const uint8_t clampedCount = (payload.count > config::common::kTeleopBatchMaxServos)
                                   ? config::common::kTeleopBatchMaxServos
                                   : payload.count;

  PresencePacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kPresenceVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::ServoControlBatch);
  packet.reserved = static_cast<uint8_t>(payload.requestId & 0xFFU);
  packet.controlOp = static_cast<uint8_t>(ServoControlOpcode::TeleopMirrorBatch);
  packet.reserved2 = payload.requestId;
  packet.controlValue = 0U;

  packet.servoTelemetry[0] = static_cast<char>(clampedCount);
  packet.servoTelemetry[1] =
      static_cast<char>(payload.speedPct | (payload.turbo ? static_cast<uint8_t>(0x80U) : 0U));
  for (uint8_t i = 0U; i < clampedCount; ++i) {
    const uint8_t offset = static_cast<uint8_t>(2U + (i * 3U));
    const uint16_t posRaw = static_cast<uint16_t>(payload.positions[i]);
    packet.servoTelemetry[offset] = static_cast<char>(payload.ids[i]);
    packet.servoTelemetry[offset + 1U] = static_cast<char>(posRaw & 0xFFU);
    packet.servoTelemetry[offset + 2U] = static_cast<char>((posRaw >> 8U) & 0xFFU);
  }

  memcpy(buffer, &packet, sizeof(packet));
  outLen = sizeof(packet);
  return true;
}

bool TeleopEspNowLegacyBatchCodec::decode(const uint8_t *buffer, size_t len, TeleopEspNowBatchPayload &payload) const {
  if (buffer == nullptr || len != sizeof(PresencePacket)) {
    return false;
  }

  PresencePacket packet{};
  memcpy(&packet, buffer, sizeof(packet));
  if (packet.magic != kPresenceMagic || packet.version != kPresenceVersion) {
    return false;
  }
  if (packet.messageType != static_cast<uint8_t>(PresenceMessageType::ServoControlBatch)) {
    return false;
  }

  return decodeLegacyBatchFromPresence(packet, payload);
}

size_t TeleopEspNowLegacyBatchCodec::encodedSize() const {
  return sizeof(PresencePacket);
}

} // namespace soarm
