#pragma once

#include "../Config/common_runtime_config.h"

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace soarm {

/// Cached follower present-load samples for pipelined ESP-NOW uplink (pattern D).
class FollowerTeleopLoadSnapshot {
public:
  void publish(const uint8_t loads[config::common::kTeleopBatchMaxServos], uint32_t capturedAtMs);
  void copyLoads(uint8_t out[config::common::kTeleopBatchMaxServos]) const;
  bool isValid() const;
  void invalidate();
  void noteSamplerSkip();
  uint32_t samplerSkipCount() const;

private:
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  uint8_t loads_[config::common::kTeleopBatchMaxServos]{};
  bool valid_{false};
  uint32_t capturedAtMs_{0U};
  uint32_t samplerSkipCount_{0U};
};

} // namespace soarm
