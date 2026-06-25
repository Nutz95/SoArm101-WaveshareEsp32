#include "follower_teleop_load_snapshot.h"

#include <cstring>

namespace soarm {

void FollowerTeleopLoadSnapshot::publish(
    const uint8_t loads[config::common::kTeleopBatchMaxServos],
    uint32_t capturedAtMs) {
  if (loads == nullptr) {
    return;
  }
  portENTER_CRITICAL(&mux_);
  memcpy(loads_, loads, sizeof(loads_));
  capturedAtMs_ = capturedAtMs;
  valid_ = true;
  portEXIT_CRITICAL(&mux_);
}

void FollowerTeleopLoadSnapshot::copyLoads(uint8_t out[config::common::kTeleopBatchMaxServos]) const {
  if (out == nullptr) {
    return;
  }
  portENTER_CRITICAL(&mux_);
  memcpy(out, loads_, sizeof(loads_));
  portEXIT_CRITICAL(&mux_);
}

bool FollowerTeleopLoadSnapshot::isValid() const {
  portENTER_CRITICAL(&mux_);
  const bool valid = valid_;
  portEXIT_CRITICAL(&mux_);
  return valid;
}

void FollowerTeleopLoadSnapshot::invalidate() {
  portENTER_CRITICAL(&mux_);
  valid_ = false;
  capturedAtMs_ = 0U;
  portEXIT_CRITICAL(&mux_);
}

void FollowerTeleopLoadSnapshot::noteSamplerSkip() {
  portENTER_CRITICAL(&mux_);
  ++samplerSkipCount_;
  portEXIT_CRITICAL(&mux_);
}

uint32_t FollowerTeleopLoadSnapshot::samplerSkipCount() const {
  portENTER_CRITICAL(&mux_);
  const uint32_t count = samplerSkipCount_;
  portEXIT_CRITICAL(&mux_);
  return count;
}

} // namespace soarm
