#pragma once

#include <cstdint>

namespace soarm {
namespace teleop_position_pack {

constexpr uint8_t kSlotCount = 6U;
constexpr uint8_t kPackedByteLength = 9U;
constexpr uint16_t kMaxPosition = 4095U;

/// Clamps a signed STS position to the 12-bit wire range (0..4095).
uint16_t clampStsPosition(int16_t position);

uint8_t popcountMask(uint8_t mask);

/// Payload byte length for a masked set of 12-bit positions.
uint8_t packedByteLengthForMask(uint8_t activeMask);

/// Packs only the slots selected by activeMask (in slot order).
void packMaskedSlots12Bit(uint8_t activeMask, const uint16_t slots[kSlotCount], uint8_t *out, uint8_t outCapacity);

/// Unpacks masked 12-bit positions into slot array; inactive slots are left unchanged.
bool unpackMaskedSlots12Bit(
    uint8_t activeMask,
    const uint8_t *in,
    uint8_t inLen,
    uint16_t slots[kSlotCount]);

/// Packs all six slots as 12-bit values (72 bits, 9 bytes). Used by unit tests.
void pack6Slots12Bit(const uint16_t slots[kSlotCount], uint8_t out[kPackedByteLength]);

/// Unpacks a fixed 9-byte buffer into all six slot positions.
void unpack6Slots12Bit(const uint8_t in[kPackedByteLength], uint16_t slots[kSlotCount]);

} // namespace teleop_position_pack
} // namespace soarm
