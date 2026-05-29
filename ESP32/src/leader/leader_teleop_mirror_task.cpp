#include "leader_teleop_mirror_task.h"

#include "leader_teleop_mirror_task_internal.h"

#include "../Config/common_runtime_config.h"
#include "../Config/leader_runtime_config.h"

#include <cstdlib>
#include "../common/calibration/calibration_profile_utils.h"
#include "../common/servo/servo_position_snapshot.h"
#include "../common/teleop/teleop_follower_endpoint.h"
#include "../common/teleop/teleop_packet_flags.h"

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

  if (static_cast<TeleopTransportMode>(transportMode.load()) != TeleopTransportMode::WifiUdp) {
    return true;
  }

  char endpoint[16]{};
  return resolveFollowerEndpoint(presenceService.followerIp(), endpoint, sizeof(endpoint));
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
  if (id == 0U || (filterId != 0U && id != filterId)) {
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

  const int32_t bounded =
      (unwrapped > config::common::kTeleopPositionClampMax)
          ? config::common::kTeleopPositionClampMax
          : ((unwrapped < config::common::kTeleopPositionClampMin)
                 ? config::common::kTeleopPositionClampMin
                 : unwrapped);
  const int16_t boundedPosition = static_cast<int16_t>(bounded);
  if (state.hasLastSentPositionById[id]) {
    const int32_t delta = boundedPosition - state.lastSentPositionById[id];
    const int32_t absDelta = (delta < 0) ? -delta : delta;
    if (absDelta < config::leader::kTeleopMirrorMinPositionDelta) {
      return;
    }
  }

  if (batchCount >= config::common::kTeleopBatchMaxServos) {
    return;
  }

  state.batchIds[batchCount] = id;
  state.batchPositions[batchCount] = boundedPosition;
  ++batchCount;
  state.lastSentPositionById[id] = boundedPosition;
  state.hasLastSentPositionById[id] = true;
}

uint8_t buildMirrorBatchFromSnapshot(
    TeleopMirrorState &state,
    const ServoPositionSnapshot &snapshot,
    const CalibrationProfile &leaderCalibrationProfile,
    const CalibrationProfile &followerCalibrationProfile,
    uint8_t filterId) {
  uint8_t batchCount = 0U;
  for (uint8_t i = 0U; i < snapshot.count; ++i) {
    tryAppendBatchItem(
        state,
        snapshot.samples[i].id,
        snapshot.samples[i].position,
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
               uint8_t speedPercent,
               bool wifiRequireAck) {
  if (count == 0U) {
    return;
  }

  ++requestCounter;
  const uint8_t wifiFlags = wifiRequireAck ? teleop::kFlagRequireAck : 0U;
  const bool sent = (transportMode == TeleopTransportMode::WifiUdp)
                        ? teleopWifiBridge.sendBatch(
                              presenceService.followerIp(),
                              state.batchIds,
                              state.batchPositions,
                              count,
                              speedPercent,
                              requestCounter,
                              wifiFlags)
                        : presenceService.requestTeleopMirrorBatch(
                              state.batchIds,
                              state.batchPositions,
                              count,
                              speedPercent,
                              requestCounter);
  if (!sent) {
    const uint8_t previous = latencyMetrics.sendFailCount.load();
    if (previous < 255U) {
      latencyMetrics.sendFailCount.store(static_cast<uint8_t>(previous + 1U));
    }
    return;
  }

  if (transportMode == TeleopTransportMode::WifiUdp && !wifiRequireAck) {
    return;
  }

  registerPendingBatch(state, requestCounter, nowMs);
  latencyMetrics.pendingCount.store(countPendingBatches(state));
}

bool canSendNowForTransport(
    const TeleopMirrorState &state,
    TeleopTransportMode transportMode,
    bool wifiRequireAck) {
  if (transportMode != TeleopTransportMode::WifiUdp || !wifiRequireAck) {
    return true;
  }

  return countPendingBatches(state) < config::leader::kTeleopWifiMaxPendingBatches;
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
    const bool wifiRequireAck = config::leader::kTeleopWifiRequireAck;

    if (selectedMode == TeleopTransportMode::WifiUdp && wifiRequireAck) {
      processWifiBatchAck(state, teleopWifiBridge, latencyMetrics, nowMs);
      expireOldPendingBatches(state, nowMs, latencyMetrics);
    } else if (selectedMode != TeleopTransportMode::WifiUdp) {
      processFollowerBatchAck(state, presenceService, latencyMetrics, nowMs);
      expireOldPendingBatches(state, nowMs, latencyMetrics);
    }

    if (!canMirrorNow(continuousEnabled, transportMode, runtimeMode, presenceService)) {
      resetHistory(state);
      vTaskDelay(pdMS_TO_TICKS(config::leader::kTeleopMirrorTaskIdleDelayMs));
      continue;
    }

    refreshFollowerIds(state, presenceService);

    ServoPositionSnapshot snapshot{};
    if (!servoBusService.copyPositionSnapshot(snapshot)) {
      vTaskDelay(pdMS_TO_TICKS(config::leader::kTeleopMirrorTaskActiveDelayMs));
      continue;
    }

    const uint8_t batchCount = buildMirrorBatchFromSnapshot(
        state,
        snapshot,
        leaderCalibrationProfile,
        followerCalibrationProfile,
        servoIdFilter.load());
    if (canSendNowForTransport(state, selectedMode, wifiRequireAck)) {
      sendBatch(
          presenceService,
          teleopWifiBridge,
          batchCount,
          state,
          requestCounter,
          latencyMetrics,
          nowMs,
          selectedMode,
          speedPct.load(),
          wifiRequireAck);
    }

    const uint32_t activeDelayMs =
        (selectedMode == TeleopTransportMode::WifiUdp)
            ? config::leader::kTeleopMirrorTaskWifiActiveDelayMs
            : config::leader::kTeleopMirrorTaskActiveDelayMs;
    vTaskDelay(pdMS_TO_TICKS(activeDelayMs));
  }
}

} // namespace soarm
