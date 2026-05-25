#pragma once

#include "../common/types/arm_runtime_state.h"
#include "../common/types/operation_mode.h"

#include <cstdint>

namespace soarm {

struct LeaderTelemetrySnapshot {
  uint32_t uptimeMs;
  uint8_t cpu0LoadPct;
  uint8_t cpu1LoadPct;
  uint8_t followerAckRetriesUsed;
  uint8_t followerAckRttMs;
  uint8_t followerAckTimeoutCount;
  uint8_t followerAckPending;
  uint8_t teleopContinuousEnabled;
  uint8_t teleopContinuousServoId;
  ArmRuntimeState leaderState;
  ArmRuntimeState followerState;
  OperationMode mode;
  bool joystickPaired;
  bool calibrationDone;
  bool espNowLinked;
  bool pairingLocked;
  bool leaderServoDebugManual;
  bool followerServoDebugManual;
  uint8_t leaderServoCount;
  uint8_t followerServoCount;
  char leaderIp[16];
  char followerIp[16];
  char leaderMac[18];
  char followerMac[18];
  char leaderServoIds[48];
  char followerServoIds[48];
  char leaderServoTelemetry[96];
  char followerServoTelemetry[96];
  uint16_t commandRequestId;
  uint8_t commandCode;
  uint8_t leaderCommandStatus;
  uint8_t followerCommandStatus;
  char status[24];
} __attribute__((packed));

} // namespace soarm
