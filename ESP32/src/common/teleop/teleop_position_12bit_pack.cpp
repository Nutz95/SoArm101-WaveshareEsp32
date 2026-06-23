#include "teleop_position_12bit_pack.h"

namespace soarm {
namespace teleop_position_pack {

// Clamps a signed STS position to the 12-bit wire range (0..4095).
uint16_t clampStsPosition(int16_t position) {
  if (position < 0) {
    return 0U;
  }
  if (position > static_cast<int16_t>(kMaxPosition)) {
    return kMaxPosition;
  }
  return static_cast<uint16_t>(position);
}

// Returns the number of set bits in activeMask (one bit per servo slot).
uint8_t popcountMask(uint8_t mask) {
  uint8_t count = 0U;
  for (uint8_t bit = 0U; bit < kSlotCount; ++bit) {
    if ((mask & (1U << bit)) != 0U) {
      ++count;
    }
  }
  return count;
}

// Returns the on-wire byte length for popcount(activeMask) values at 12 bits each.
uint8_t packedByteLengthForMask(uint8_t activeMask) {
  const uint8_t activeCount = popcountMask(activeMask);
  return static_cast<uint8_t>((static_cast<uint16_t>(activeCount) * 12U + 7U) / 8U);
}

namespace {

void packValues12Bit(const uint16_t *values, uint8_t valueCount, uint8_t *out, uint8_t outCapacity) {
  if (values == nullptr || out == nullptr || valueCount == 0U) {
    return;
  }

  uint32_t bitOffset = 0U;
  for (uint8_t index = 0U; index < valueCount; ++index) {
    const uint32_t value = static_cast<uint32_t>(values[index] & kMaxPosition);
    for (uint8_t bit = 0U; bit < 12U; ++bit) {
      const uint32_t byteIndex = bitOffset / 8U;
      if (byteIndex >= outCapacity) {
        return;
      }
      if ((value >> bit) & 0x01U) {
        out[byteIndex] |= static_cast<uint8_t>(1U << (bitOffset % 8U));
      }
      ++bitOffset;
    }
  }
}

void unpackValues12Bit(const uint8_t *in, uint8_t inLen, uint8_t valueCount, uint16_t *values) {
  if (in == nullptr || values == nullptr || valueCount == 0U) {
    return;
  }

  uint32_t bitOffset = 0U;
  for (uint8_t index = 0U; index < valueCount; ++index) {
    uint32_t value = 0U;
    for (uint8_t bit = 0U; bit < 12U; ++bit) {
      const uint32_t byteIndex = bitOffset / 8U;
      if (byteIndex >= inLen) {
        break;
      }
      if ((in[byteIndex] >> (bitOffset % 8U)) & 0x01U) {
        value |= (1UL << bit);
      }
      ++bitOffset;
    }
    values[index] = static_cast<uint16_t>(value & kMaxPosition);
  }
}

} // namespace

// Packs only the slots selected by activeMask, in ascending slot order.
void packMaskedSlots12Bit(uint8_t activeMask, const uint16_t slots[kSlotCount], uint8_t *out, uint8_t outCapacity) {
  if (slots == nullptr || out == nullptr) {
    return;
  }

  uint16_t values[kSlotCount]{};
  uint8_t valueCount = 0U;
  for (uint8_t slot = 0U; slot < kSlotCount; ++slot) {
    if ((activeMask & (1U << slot)) == 0U) {
      continue;
    }
    values[valueCount] = slots[slot];
    ++valueCount;
  }
  packValues12Bit(values, valueCount, out, outCapacity);
}

// Unpacks masked 12-bit positions into slots; inactive slots are left unchanged.
bool unpackMaskedSlots12Bit(
    uint8_t activeMask,
    const uint8_t *in,
    uint8_t inLen,
    uint16_t slots[kSlotCount]) {
  if (in == nullptr || slots == nullptr) {
    return false;
  }

  const uint8_t expectedLen = packedByteLengthForMask(activeMask);
  if (inLen < expectedLen) {
    return false;
  }

  uint16_t values[kSlotCount]{};
  const uint8_t valueCount = popcountMask(activeMask);
  unpackValues12Bit(in, inLen, valueCount, values);

  uint8_t valueIndex = 0U;
  for (uint8_t slot = 0U; slot < kSlotCount; ++slot) {
    if ((activeMask & (1U << slot)) == 0U) {
      continue;
    }
    slots[slot] = values[valueIndex];
    ++valueIndex;
  }
  return true;
}

// Packs all six slots as 12-bit values (72 bits, 9 bytes). Used by unit tests.
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

// Unpacks a fixed 9-byte buffer into all six slot positions.
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
