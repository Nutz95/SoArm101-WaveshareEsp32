#pragma once

#include "teleop_espnow_batch_codec.h"

namespace soarm {

class TeleopEspNowLegacyBatchCodec final : public ITeleopEspNowBatchCodec {
public:
  bool encode(
      const TeleopEspNowBatchPayload &payload,
      uint8_t *buffer,
      size_t capacity,
      size_t &outLen) const override;

  bool decode(const uint8_t *buffer, size_t len, TeleopEspNowBatchPayload &payload) const override;

  size_t encodedSize() const override;
};

} // namespace soarm
