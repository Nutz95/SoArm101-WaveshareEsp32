#pragma once

#include "../common/types/arm_runtime_state.h"
#include "../common/types/calibration_profile.h"
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
  uint8_t teleopMirrorLatencyLastMs;
  uint8_t teleopMirrorLatencyEwmaMs;
  uint8_t teleopMirrorLatencyP95Ms;
  uint8_t teleopMirrorPendingCount;
  uint8_t teleopMirrorTimeoutCount;
  uint8_t teleopContinuousEnabled;
  uint8_t teleopContinuousServoId;
  uint8_t teleopTransportMode;
  uint8_t xboxRuntimeState;
  uint16_t xboxLastReportAgeMs;
  uint16_t xboxReportCount;
  uint16_t xboxButtonsMask;
  int16_t xboxAxisLeftX;
  int16_t xboxAxisLeftY;
  int16_t xboxAxisRightX;
  int16_t xboxAxisRightY;
  int16_t xboxDpadX;
  int16_t xboxDpadY;
  uint8_t xboxTriggerLeft;
  uint8_t xboxTriggerRight;
  ArmRuntimeState leaderState;
  ArmRuntimeState followerState;
  OperationMode mode;
  bool xboxLinkEncrypted;
  bool xboxInputSubscribed;
  bool joystickPaired;
  bool xboxControllerPaired;
  bool calibrationDone;
  bool espNowLinked;
  bool pairingLocked;
  bool leaderServoDebugManual;
  bool followerServoDebugManual;
  bool leaderServoTemperatureAlarm;
  bool followerServoTemperatureAlarm;
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
  char xboxControllerName[32];
  uint16_t commandRequestId;
  uint8_t commandCode;
  uint8_t leaderCommandStatus;
  uint8_t followerCommandStatus;
  char status[24];
  // Controller operation profile: 0=cal_leader, 1=cal_follower, 2=teleop_espnow, 3=teleop_wifi
  uint8_t controllerOperationProfile;
  // NVS-stored calibration limits for both arms (transmitted so the dashboard can display them)
  uint16_t leaderCalibrationMin[CalibrationProfile::kServoCount];
  uint16_t leaderCalibrationMax[CalibrationProfile::kServoCount];
  uint16_t followerCalibrationMin[CalibrationProfile::kServoCount];
  uint16_t followerCalibrationMax[CalibrationProfile::kServoCount];
} __attribute__((packed));

} // namespace soarm
