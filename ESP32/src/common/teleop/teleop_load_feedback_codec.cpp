#include "teleop_load_feedback_codec.h"

#include <cstring>

namespace soarm {
namespace teleop_load_feedback {

namespace {

constexpr uint8_t kLoadScaleDivisor = 8U;

struct TeleopLoadFeedbackPacket {
  uint8_t magic;
  uint8_t version;
  uint8_t messageType;
  uint16_t requestId;
  uint8_t load[6];
} __attribute__((packed));

static_assert(sizeof(TeleopLoadFeedbackPacket) == kLoadFeedbackWireSize, "load feedback wire size");

} // namespace

uint8_t encodeLoadWire(int16_t rawLoad) {
  int32_t magnitude = rawLoad;
  if (magnitude < 0) {
    magnitude = -magnitude;
  }
  if (magnitude < static_cast<int32_t>(kLoadScaleDivisor)) {
    return 0U;
  }
  const uint32_t scaled = static_cast<uint32_t>(magnitude) / kLoadScaleDivisor;
  return scaled > kLoadWireMax ? kLoadWireMax : static_cast<uint8_t>(scaled);
}

int16_t decodeLoadWire(uint8_t wireLoad) {
  return static_cast<int16_t>(static_cast<uint32_t>(wireLoad) * kLoadScaleDivisor);
}

bool isTeleopLoadFeedbackPacket(const uint8_t *data, size_t len) {
  return data != nullptr && len == kLoadFeedbackWireSize && data[0] == kPresenceMagic &&
         data[1] == kLoadFeedbackWireVersion &&
         data[2] == static_cast<uint8_t>(PresenceMessageType::TeleopLoadFeedback);
}

bool encodePacket(
    uint16_t requestId,
    const uint8_t loads[6],
    uint8_t *out,
    size_t outCapacity,
    size_t &outLen) {
  if (out == nullptr || loads == nullptr || outCapacity < kLoadFeedbackWireSize) {
    return false;
  }

  TeleopLoadFeedbackPacket packet{};
  packet.magic = kPresenceMagic;
  packet.version = kLoadFeedbackWireVersion;
  packet.messageType = static_cast<uint8_t>(PresenceMessageType::TeleopLoadFeedback);
  packet.requestId = requestId;
  memcpy(packet.load, loads, sizeof(packet.load));

  memcpy(out, &packet, sizeof(packet));
  outLen = sizeof(packet);
  return true;
}

bool decodePacket(const uint8_t *data, size_t len, uint16_t &requestId, uint8_t loads[6]) {
  if (!isTeleopLoadFeedbackPacket(data, len) || loads == nullptr) {
    return false;
  }

  TeleopLoadFeedbackPacket packet{};
  memcpy(&packet, data, sizeof(packet));
  requestId = packet.requestId;
  for (uint8_t i = 0U; i < 6U; ++i) {
    loads[i] = static_cast<uint8_t>(packet.load[i] & 0x7FU);
  }
  return true;
}

} // namespace teleop_load_feedback
} // namespace soarm
