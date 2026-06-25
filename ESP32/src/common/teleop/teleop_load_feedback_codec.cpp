#include "teleop_load_feedback_codec.h"

#include <cstring>

namespace soarm {
namespace teleop_load_feedback {

int8_t encodeLoadWire(int16_t rawLoad) {
  int32_t scaled = static_cast<int32_t>(rawLoad) / 10;
  if (scaled > 127) {
    return 127;
  }
  if (scaled < -128) {
    return -128;
  }
  return static_cast<int8_t>(scaled);
}

int16_t decodeLoadWire(int8_t wireLoad) {
  return static_cast<int16_t>(static_cast<int32_t>(wireLoad) * 10);
}

bool isTeleopLoadFeedbackPacket(const uint8_t *data, size_t len) {
  if (data == nullptr || len != kLoadFeedbackWireSize) {
    return false;
  }
  if (data[0] != kPresenceMagic) {
    return false;
  }
  if (data[1] != kLoadFeedbackPacketVersion) {
    return false;
  }
  return data[2] == static_cast<uint8_t>(PresenceMessageType::TeleopLoadFeedback);
}

bool encodePacket(
    uint16_t requestId,
    uint8_t seq,
    const int8_t loads[6],
    uint8_t *out,
    size_t outCapacity,
    size_t &outLen) {
  if (out == nullptr || loads == nullptr || outCapacity < kLoadFeedbackWireSize) {
    return false;
  }

  TeleopLoadFeedbackPacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kLoadFeedbackPacketVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::TeleopLoadFeedback);
  packet.requestId = requestId;
  packet.seq = seq;
  memcpy(packet.load, loads, sizeof(packet.load));

  memcpy(out, &packet, sizeof(packet));
  outLen = sizeof(packet);
  return true;
}

bool decodePacket(
    const uint8_t *data,
    size_t len,
    uint16_t &requestId,
    uint8_t &seq,
    int8_t loads[6]) {
  if (!isTeleopLoadFeedbackPacket(data, len) || loads == nullptr) {
    return false;
  }

  TeleopLoadFeedbackPacket packet{};
  memcpy(&packet, data, sizeof(packet));
  requestId = packet.requestId;
  seq = packet.seq;
  memcpy(loads, packet.load, sizeof(packet.load));
  return true;
}

} // namespace teleop_load_feedback
} // namespace soarm
