#pragma once

#include "../presence/presence_constants.h"
#include "../presence/presence_message_type.h"

#include <cstddef>
#include <cstdint>

namespace soarm {
namespace teleop_load_feedback {

constexpr uint8_t kLoadFeedbackPacketVersion = 1U;
constexpr size_t kLoadFeedbackWireSize = 12U;

struct TeleopLoadFeedbackPacket {
  uint8_t magic;
  uint8_t version;
  uint8_t messageType;
  uint16_t requestId;
  uint8_t seq;
  int8_t load[6];
} __attribute__((packed));

static_assert(sizeof(TeleopLoadFeedbackPacket) == kLoadFeedbackWireSize, "load feedback wire size");

/// Maps STS present load (0..1000) to a compact signed wire value.
int8_t encodeLoadWire(int16_t rawLoad);

/// Expands wire load back to an approximate STS-scale value (tests / debug).
int16_t decodeLoadWire(int8_t wireLoad);

bool isTeleopLoadFeedbackPacket(const uint8_t *data, size_t len);

bool encodePacket(
    uint16_t requestId,
    uint8_t seq,
    const int8_t loads[6],
    uint8_t *out,
    size_t outCapacity,
    size_t &outLen);

bool decodePacket(
    const uint8_t *data,
    size_t len,
    uint16_t &requestId,
    uint8_t &seq,
    int8_t loads[6]);

} // namespace teleop_load_feedback
} // namespace soarm
