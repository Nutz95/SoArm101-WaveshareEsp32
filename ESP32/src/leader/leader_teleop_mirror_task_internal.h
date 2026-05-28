#pragma once

#include "leader_teleop_mirror_task.h"

#include "../common/interfaces/i_leader_presence_service.h"
#include "../common/servo/servo_control_opcode.h"
#include "leader_teleop_wifi_bridge.h"

#include <cstdint>

namespace soarm {

struct MirrorSample {
  uint8_t id;
  int16_t position;
};

struct PendingBatch {
  uint16_t requestId;
  uint32_t sentAtMs;
  bool active;
};

struct TeleopMirrorState {
  bool followerIds[256]{};
  bool hasFollowerIds{false};
  bool parsedFollowerIds[256]{};
  MirrorSample mirrorSamples[16]{};
  int16_t previousRawById[256]{};
  int32_t unwrappedById[256]{};
  bool hasPreviousById[256]{};
  int16_t lastSentPositionById[256]{};
  bool hasLastSentPositionById[256]{};
  uint8_t batchIds[6]{};
  int16_t batchPositions[6]{};
  PendingBatch pendingBatches[16]{};
  uint8_t pendingWriteIndex{0U};
  uint8_t latencySamples[32]{};
  uint8_t latencySampleCount{0U};
  uint8_t latencySampleWriteIndex{0U};
  bool hasProcessedAck{false};
  uint16_t lastProcessedAckRequestId{0U};
  uint8_t lastProcessedAckCommandOp{0U};
};

uint8_t parseMirrorSamples(const char *telemetry, MirrorSample *out, uint8_t capacity);
uint8_t parseIdList(const char *idsText, bool present[256]);
int32_t unwrapPosition(int16_t rawPosition, int16_t previousRaw, int32_t previousUnwrapped);
void resetHistory(TeleopMirrorState &state);
void registerPendingBatch(TeleopMirrorState &state, uint16_t requestId, uint32_t nowMs);
uint8_t countPendingBatches(const TeleopMirrorState &state);
void processFollowerBatchAck(TeleopMirrorState &state, ILeaderPresenceService &presenceService, TeleopMirrorLatencyMetrics &latencyMetrics, uint32_t nowMs);
void processWifiBatchAck(TeleopMirrorState &state, LeaderTeleopWifiBridge &teleopWifiBridge, TeleopMirrorLatencyMetrics &latencyMetrics, uint32_t nowMs);
void expireOldPendingBatches(TeleopMirrorState &state, uint32_t nowMs, TeleopMirrorLatencyMetrics &latencyMetrics);

} // namespace soarm