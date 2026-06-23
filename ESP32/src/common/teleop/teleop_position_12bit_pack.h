#pragma once

#include <cstdint>

namespace soarm {
namespace teleop_position_pack {

constexpr uint8_t kSlotCount = 6U;
constexpr uint8_t kPackedByteLength = 9U;
constexpr uint16_t kMaxPosition = 4095U;

uint16_t clampStsPosition(int16_t position);

void pack6Slots12Bit(const uint16_t slots[kSlotCount], uint8_t out[kPackedByteLength]);

void unpack6Slots12Bit(const uint8_t in[kPackedByteLength], uint16_t slots[kSlotCount]);

} // namespace teleop_position_pack
} // namespace soarm
