#include "leader_teleop_mirror_task.h"

#include "leader_teleop_mirror_task_internal.h"

#include "../Config/leader_runtime_config.h"
#include "../common/calibration/calibration_profile_utils.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

namespace soarm {

namespace {

bool canMirrorNow(
    const std::atomic<bool> &continuousEnabled,
    const std::atomic<uint8_t> &transportMode,
    const std::atomic<uint8_t> &runtimeMode,
    const ILeaderPresenceService &presenceService) {
  const bool active = continuousEnabled.load();
  const OperationMode mode = static_cast<OperationMode>(runtimeMode.load());
  if (!(active && mode == OperationMode::Teleoperation && presenceService.isFollowerLinked())) {
    return false;
  }

  return static_cast<TeleopTransportMode>(transportMode.load()) != TeleopTransportMode::WifiUdp ||
         presenceService.hasValidFollowerIp();
}

void refreshFollowerIds(TeleopMirrorState &state, ILeaderPresenceService &presenceService) {
  const uint8_t parsedCount = parseIdList(presenceService.followerServoIds(), state.parsedFollowerIds);
  if (parsedCount > 0U) {
    memcpy(state.followerIds, state.parsedFollowerIds, sizeof(state.followerIds));
    state.hasFollowerIds = true;
  }
}

void tryAppendBatchItem(
    TeleopMirrorState &state,
    uint8_t id,
    int16_t rawPosition,
    const CalibrationProfile &leaderCalibrationProfile,
    const CalibrationProfile &followerCalibrationProfile,
    uint8_t &batchCount,
    uint8_t filterId) {
  if (id == 0U || (filterId != 0U && id != filterId) || !state.hasFollowerIds || !state.followerIds[id]) {
    return;
  }

  const int16_t calibratedPosition = remapServoPositionWithCalibration(
      id,
      rawPosition,
      leaderCalibrationProfile,
      followerCalibrationProfile);

  const int32_t unwrapped = state.hasPreviousById[id]
                                ? unwrapPosition(calibratedPosition, state.previousRawById[id], state.unwrappedById[id])
                                : static_cast<int32_t>(calibratedPosition);

  state.previousRawById[id] = calibratedPosition;
  state.unwrappedById[id] = unwrapped;
  state.hasPreviousById[id] = true;

  const int32_t bounded = (unwrapped > 32767) ? 32767 : ((unwrapped < -32767) ? -32767 : unwrapped);
  const int16_t boundedPosition = static_cast<int16_t>(bounded);
  const bool changed = !state.hasLastSentPositionById[id] || state.lastSentPositionById[id] != boundedPosition;
  if (!changed || batchCount >= 6U) {
    return;
  }

  state.batchIds[batchCount] = id;
  state.batchPositions[batchCount] = boundedPosition;
  ++batchCount;
  state.lastSentPositionById[id] = boundedPosition;
  state.hasLastSentPositionById[id] = true;
}

uint8_t buildMirrorBatch(
    TeleopMirrorState &state,
    ServoBusService &servoBusService,
    const CalibrationProfile &leaderCalibrationProfile,
    const CalibrationProfile &followerCalibrationProfile,
    uint8_t filterId) {
  const uint8_t sampleCount = parseMirrorSamples(
      servoBusService.lastTelemetryText(),
      state.mirrorSamples,
      static_cast<uint8_t>(sizeof(state.mirrorSamples) / sizeof(state.mirrorSamples[0])));

  uint8_t batchCount = 0U;
  for (uint8_t i = 0U; i < sampleCount; ++i) {
    tryAppendBatchItem(
        state,
        state.mirrorSamples[i].id,
        state.mirrorSamples[i].position,
        leaderCalibrationProfile,
        followerCalibrationProfile,
        batchCount,
        filterId);
  }
  return batchCount;
}

void sendBatch(ILeaderPresenceService &presenceService,
               LeaderTeleopWifiBridge &teleopWifiBridge,
               uint8_t count,
               TeleopMirrorState &state,
               uint16_t &requestCounter,
               TeleopMirrorLatencyMetrics &latencyMetrics,
               uint32_t nowMs,
               TeleopTransportMode transportMode,
               uint8_t speedPercent) {
  if (count == 0U) {
    return;
  }

  ++requestCounter;
  const bool sent = (transportMode == TeleopTransportMode::WifiUdp)
                        ? teleopWifiBridge.sendBatch(
                              presenceService.followerIp(),
                              state.batchIds,
                              state.batchPositions,
                              count,
                              speedPercent,
                              requestCounter)
                        : presenceService.requestTeleopMirrorBatch(
                              state.batchIds,
                              state.batchPositions,
                              count,
                              speedPercent,
                              requestCounter);
  if (sent) {
    registerPendingBatch(state, requestCounter, nowMs);
    latencyMetrics.pendingCount.store(countPendingBatches(state));
  }
}

} // namespace

void LeaderTeleopMirrorTask::runLoop(ServoBusService &servoBusService,
                                     ILeaderPresenceService &presenceService,
                                     LeaderTeleopWifiBridge &teleopWifiBridge,
                                     const CalibrationProfile &leaderCalibrationProfile,
                                     const CalibrationProfile &followerCalibrationProfile,
                                     const std::atomic<bool> &continuousEnabled,
                                     const std::atomic<uint8_t> &servoIdFilter,
                                     const std::atomic<uint8_t> &speedPct,
                                     const std::atomic<uint8_t> &transportMode,
                                     const std::atomic<uint8_t> &runtimeMode,
                                     uint16_t &requestCounter,
                                     TeleopMirrorLatencyMetrics &latencyMetrics) {
  static TeleopMirrorState state{};

  while (true) {
    const uint32_t nowMs = millis();
    const TeleopTransportMode selectedMode = static_cast<TeleopTransportMode>(transportMode.load());
    if (selectedMode == TeleopTransportMode::WifiUdp) {
      processWifiBatchAck(state, teleopWifiBridge, latencyMetrics, nowMs);
    } else {
      processFollowerBatchAck(state, presenceService, latencyMetrics, nowMs);
    }
    expireOldPendingBatches(state, nowMs, latencyMetrics);

    if (!canMirrorNow(continuousEnabled, transportMode, runtimeMode, presenceService)) {
      resetHistory(state);
      vTaskDelay(pdMS_TO_TICKS(config::leader::kTeleopMirrorTaskIdleDelayMs));
      continue;
    }

    refreshFollowerIds(state, presenceService);
    const uint8_t batchCount = buildMirrorBatch(
        state,
        servoBusService,
        leaderCalibrationProfile,
        followerCalibrationProfile,
        servoIdFilter.load());
    sendBatch(
        presenceService,
        teleopWifiBridge,
        batchCount,
        state,
        requestCounter,
        latencyMetrics,
        nowMs,
        selectedMode,
        speedPct.load());

    vTaskDelay(pdMS_TO_TICKS(config::leader::kTeleopMirrorTaskActiveDelayMs));
  }
}

} // namespace soarm
