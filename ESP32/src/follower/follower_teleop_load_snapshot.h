#pragma once

#include "../Config/common_runtime_config.h"

#include <cstdint>

namespace soarm {

/// Cached follower present-load samples for pipelined ESP-NOW uplink (pattern D).
class FollowerTeleopLoadSnapshot {
public:
  void publish(const int8_t loads[config::common::kTeleopBatchMaxServos], uint32_t capturedAtMs);
  void copyLoads(int8_t out[config::common::kTeleopBatchMaxServos]) const;
  bool isValid() const;
  void invalidate();
  void noteSamplerSkip();
  uint32_t samplerSkipCount() const;

private:
  int8_t loads_[config::common::kTeleopBatchMaxServos]{};
  bool valid_{false};
  uint32_t capturedAtMs_{0U};
  uint32_t samplerSkipCount_{0U};
};

} // namespace soarm
