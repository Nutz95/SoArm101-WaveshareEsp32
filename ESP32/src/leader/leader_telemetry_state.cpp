#include "leader_telemetry_state.h"

#include <cstring>

namespace soarm {

LeaderTelemetryState::LeaderTelemetryState() {
  memset(&snapshot_, 0, sizeof(snapshot_));
}

void LeaderTelemetryState::update(const LeaderTelemetrySnapshot &snapshot) {
  if (!lockManager_.lock(LockDomain::Telemetry)) {
    return;
  }

  snapshot_ = snapshot;
  lockManager_.unlock(LockDomain::Telemetry);
}

LeaderTelemetrySnapshot LeaderTelemetryState::snapshot() const {
  LeaderTelemetrySnapshot copy{};
  if (!lockManager_.lock(LockDomain::Telemetry)) {
    return copy;
  }

  copy = snapshot_;
  lockManager_.unlock(LockDomain::Telemetry);
  return copy;
}

} // namespace soarm
