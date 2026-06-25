#include "follower_teleop_load_snapshot.h"

#include <cstring>

namespace soarm {

void FollowerTeleopLoadSnapshot::publish(
    const int8_t loads[config::common::kTeleopBatchMaxServos],
    uint32_t capturedAtMs) {
  if (loads == nullptr) {
    return;
  }
  memcpy(loads_, loads, sizeof(loads_));
  capturedAtMs_ = capturedAtMs;
  valid_ = true;
}

void FollowerTeleopLoadSnapshot::copyLoads(int8_t out[config::common::kTeleopBatchMaxServos]) const {
  if (out == nullptr) {
    return;
  }
  memcpy(out, loads_, sizeof(loads_));
}

bool FollowerTeleopLoadSnapshot::isValid() const {
  return valid_;
}

void FollowerTeleopLoadSnapshot::invalidate() {
  valid_ = false;
  capturedAtMs_ = 0U;
}

void FollowerTeleopLoadSnapshot::noteSamplerSkip() {
  ++samplerSkipCount_;
}

uint32_t FollowerTeleopLoadSnapshot::samplerSkipCount() const {
  return samplerSkipCount_;
}

} // namespace soarm
