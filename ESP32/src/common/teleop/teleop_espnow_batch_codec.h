#pragma once

#include "teleop_espnow_batch_payload.h"

#include <cstddef>
#include <cstdint>

namespace soarm {

class ITeleopEspNowBatchCodec {
public:
  virtual ~ITeleopEspNowBatchCodec() = default;

  virtual bool encode(
      const TeleopEspNowBatchPayload &payload,
      uint8_t *buffer,
      size_t capacity,
      size_t &outLen) const = 0;

  virtual bool decode(const uint8_t *buffer, size_t len, TeleopEspNowBatchPayload &payload) const = 0;

  virtual size_t encodedSize() const = 0;
};

} // namespace soarm
