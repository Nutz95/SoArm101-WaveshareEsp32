#pragma once

#include <cstdint>

namespace soarm {
namespace teleop_espnow {

// Keyframe cadence for turbo sparse wire format (phase B).
constexpr uint32_t kTurboKeyframeIntervalMs = 125U;
constexpr uint8_t kTurboKeyframeEveryNFrames = 10U;

constexpr uint8_t kTurboPacketVersionV2 = 2U;
constexpr uint8_t kTurboKeyframeFlag = 0x80U;
constexpr uint8_t kTurboSpeedPctMask = 0x7FU;

} // namespace teleop_espnow
} // namespace soarm
