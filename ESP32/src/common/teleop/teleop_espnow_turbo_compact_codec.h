#pragma once

#include "teleop_espnow_batch_codec.h"
#include "teleop_espnow_turbo_session.h"

#include <cstdint>

namespace soarm {

/// Turbo ESP-NOW codec: v2 sparse/keyframe wire format (9-16 B variable length).
class TeleopEspNowTurboCompactCodec final : public ITeleopEspNowBatchCodec {
public:
  bool encode(
      const TeleopEspNowBatchPayload &payload,
      uint8_t *buffer,
      size_t capacity,
      size_t &outLen) const override;

  bool decode(const uint8_t *buffer, size_t len, TeleopEspNowBatchPayload &payload) const override;

  size_t encodedSize() const override;

  /// Stateful encode: merges payload into session, picks keyframe vs delta, writes v2 packet.
  bool encodeWithSession(
      const TeleopEspNowBatchPayload &payload,
      TeleopEspNowTurboSession &session,
      uint32_t nowMs,
      uint8_t *buffer,
      size_t capacity,
      size_t &outLen) const;

  /// Stateful decode: updates session slot cache and fills a batch payload for teleop apply.
  bool decodeWithSession(
      const uint8_t *buffer,
      size_t len,
      TeleopEspNowTurboSession &session,
      TeleopEspNowBatchPayload &payload) const;
};

} // namespace soarm
