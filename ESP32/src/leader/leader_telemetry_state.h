#pragma once

#include "../common/lock_manager.h"
#include "../common/types/arm_runtime_state.h"
#include "../common/types/operation_mode.h"

#include <cstdint>

namespace soarm {

struct LeaderTelemetrySnapshot {
  uint32_t uptimeMs;
  uint8_t cpu0LoadPct;
  uint8_t cpu1LoadPct;
  uint8_t reserved0;
  uint8_t reserved1;
  ArmRuntimeState leaderState;
  ArmRuntimeState followerState;
  OperationMode mode;
  bool joystickPaired;
  bool calibrationDone;
  bool espNowLinked;
  char leaderIp[16];
  char followerIp[16];
  char status[24];
} __attribute__((packed));

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
