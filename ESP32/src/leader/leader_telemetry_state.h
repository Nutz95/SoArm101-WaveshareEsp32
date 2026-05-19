#pragma once

#include "../common/lock_manager.h"
#include "leader_telemetry_snapshot.h"

#include <cstdint>

namespace soarm {

class LeaderTelemetryState {
public:
  LeaderTelemetryState();

  void update(const LeaderTelemetrySnapshot &snapshot);
  LeaderTelemetrySnapshot snapshot() const;

private:
  mutable LockManager lockManager_;
  LeaderTelemetrySnapshot snapshot_;
};

} // namespace soarm
