#include "teleop_position_12bit_pack.h"

namespace soarm {
namespace teleop_position_pack {

uint16_t clampStsPosition(int16_t position) {
  if (position < 0) {
    return 0U;
  }
  if (position > static_cast<int16_t>(kMaxPosition)) {
    return kMaxPosition;
  }
  return static_cast<uint16_t>(position);
}

void pack6Slots12Bit(const uint16_t slots[kSlotCount], uint8_t out[kPackedByteLength]) {
  for (uint8_t i = 0U; i < kPackedByteLength; ++i) {
    out[i] = 0U;
  }

  uint32_t bitOffset = 0U;
  for (uint8_t slot = 0U; slot < kSlotCount; ++slot) {
    const uint32_t value = static_cast<uint32_t>(slots[slot] & kMaxPosition);
    for (uint8_t bit = 0U; bit < 12U; ++bit) {
      if ((value >> bit) & 0x01U) {
        out[bitOffset / 8U] |= static_cast<uint8_t>(1U << (bitOffset % 8U));
      }
      ++bitOffset;
    }
  }
}

void unpack6Slots12Bit(const uint8_t in[kPackedByteLength], uint16_t slots[kSlotCount]) {
  uint32_t bitOffset = 0U;
  for (uint8_t slot = 0U; slot < kSlotCount; ++slot) {
    uint32_t value = 0U;
    for (uint8_t bit = 0U; bit < 12U; ++bit) {
      if ((in[bitOffset / 8U] >> (bitOffset % 8U)) & 0x01U) {
        value |= (1UL << bit);
      }
      ++bitOffset;
    }
    slots[slot] = static_cast<uint16_t>(value & kMaxPosition);
  }
}

} // namespace teleop_position_pack
} // namespace soarm
