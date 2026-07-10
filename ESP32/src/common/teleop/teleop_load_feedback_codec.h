#pragma once

#include "../presence/presence_constants.h"
#include "../presence/presence_message_type.h"

#include <cstddef>
#include <cstdint>

namespace soarm {
namespace teleop_load_feedback {

/// Byte 1 of the wire header — bump only when the on-wire layout changes (reflash leader + follower).
constexpr uint8_t kLoadFeedbackWireVersion = 1U;
constexpr size_t kLoadFeedbackWireSize = 13U;
constexpr uint8_t kLoadWireMax = 127U;

/// Maps STS present load (0..1000) to unsigned wire units 0..127 (raw / 8).
uint8_t encodeLoadWire(int16_t rawLoad);

/// Expands wire load back to an approximate STS-scale value (tests / debug).
int16_t decodeLoadWire(uint8_t wireLoad);

/// Idle gripper offset only — baseline must not chase sustained contact load.
constexpr uint8_t kGripperBaselineAdaptMaxWireLoad = 8U;

/// Subtract slow gripper idle baseline so contact force is net of mechanical offset.
uint8_t netGripperLoadWire(uint8_t wireLoad, uint8_t &baselineEwma);

bool isTeleopLoadFeedbackPacket(const uint8_t *data, size_t len);

bool encodePacket(
    uint16_t requestId,
    const uint8_t loads[6],
    uint16_t gripperPresentPos,
    uint8_t *out,
    size_t outCapacity,
    size_t &outLen);

bool decodePacket(
    const uint8_t *data,
    size_t len,
    uint16_t &requestId,
    uint8_t loads[6],
    uint16_t &gripperPresentPos);

} // namespace teleop_load_feedback
} // namespace soarm
