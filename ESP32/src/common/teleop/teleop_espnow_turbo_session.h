#pragma once

#include "teleop_espnow_batch_payload.h"
#include "teleop_position_12bit_pack.h"

#include <cstdint>

namespace soarm {

/// Last-known absolute STS positions per servo slot (IDs 1..6) for turbo sparse encode/decode.
struct TeleopEspNowTurboSession {
  uint16_t slots[teleop_position_pack::kSlotCount]{};
  uint8_t knownMask{0U};
  uint8_t framesSinceKeyframe{0U};
  uint32_t lastKeyframeMs{0U};
  bool hasBaselineKeyframe{false};

  /// Clears slot cache and keyframe cadence (call when teleop mirror starts or transport resets).
  void reset();

  /// Merges incoming batch positions into the slot cache and updates knownMask.
  void mergePayload(const TeleopEspNowBatchPayload &payload);

  /// Bit mask of slots present in this batch (servo IDs 1..6 map to bits 0..5).
  uint8_t maskFromPayload(const TeleopEspNowBatchPayload &payload) const;

  /// Builds a TeleopEspNowBatchPayload from cached slots for the given activeMask.
  void buildApplyPayload(
      uint8_t activeMask,
      uint16_t requestId,
      uint8_t speedPct,
      TeleopEspNowBatchPayload &out) const;

  /// Updates keyframe cadence counters after a frame is sent.
  void noteFrameSent(bool keyframe, uint32_t nowMs);
};

} // namespace soarm
