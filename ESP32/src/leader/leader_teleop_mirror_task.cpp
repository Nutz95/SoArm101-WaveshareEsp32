#include "leader_teleop_mirror_task.h"

#include "../Config/leader_runtime_config.h"
#include "../common/servo/servo_control_opcode.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>
#include <cstring>

namespace soarm {

namespace {

struct MirrorSample {
  uint8_t id;
  int16_t position;
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
};

uint8_t parseMirrorSamples(const char *telemetry, MirrorSample *out, uint8_t capacity) {
  if (telemetry == nullptr || out == nullptr || capacity == 0U) {
    return 0U;
  }

  uint8_t count = 0U;
  const char *cursor = telemetry;
  while (*cursor != '\0' && count < capacity) {
    while (*cursor != '\0' && *cursor != '#') {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }

    unsigned int id = 0U;
    int position = 0;
    if (sscanf(cursor, "#%u p%d", &id, &position) == 2 && id <= 255U) {
      out[count].id = static_cast<uint8_t>(id & 0xFFU);
      out[count].position = static_cast<int16_t>(position);
      ++count;
    }

    while (*cursor != '\0' && *cursor != ';' && *cursor != '#') {
      ++cursor;
    }
    if (*cursor == ';') {
      ++cursor;
    }
  }

  return count;
}

uint8_t parseIdList(const char *idsText, bool present[256]) {
  for (uint16_t i = 0; i < 256U; ++i) {
    present[i] = false;
  }

  if (idsText == nullptr) {
    return 0U;
  }

  uint8_t count = 0U;
  const char *cursor = idsText;
  while (*cursor != '\0') {
    unsigned int id = 0U;
    if (sscanf(cursor, "%u", &id) == 1 && id < 256U) {
      if (!present[id]) {
        present[id] = true;
        ++count;
      }
    }
    while (*cursor != '\0' && *cursor != ',') {
      ++cursor;
    }
    if (*cursor == ',') {
      ++cursor;
    }
  }

  return count;
}

int32_t unwrapPosition(int16_t rawPosition, int16_t previousRaw, int32_t previousUnwrapped) {
  int32_t delta = static_cast<int32_t>(rawPosition) - static_cast<int32_t>(previousRaw);
  if (delta > 2048) {
    delta -= 4096;
  } else if (delta < -2048) {
    delta += 4096;
  }
  return previousUnwrapped + delta;
}

void resetHistory(TeleopMirrorState &state) {
  for (uint16_t i = 0; i < 256U; ++i) {
    state.hasPreviousById[i] = false;
    state.hasLastSentPositionById[i] = false;
  }
}

bool canMirrorNow(
    const std::atomic<bool> &continuousEnabled,
    const std::atomic<uint8_t> &runtimeMode,
    const ILeaderPresenceService &presenceService) {
  const bool active = continuousEnabled.load();
  const OperationMode mode = static_cast<OperationMode>(runtimeMode.load());
  return active && mode == OperationMode::Teleoperation && presenceService.isFollowerLinked();
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
    uint8_t &batchCount,
    uint8_t filterId) {
  if (id == 0U || (filterId != 0U && id != filterId)) {
    return;
  }

  if (!state.hasFollowerIds || !state.followerIds[id]) {
    return;
  }

  const int32_t unwrapped = state.hasPreviousById[id]
                                ? unwrapPosition(rawPosition, state.previousRawById[id], state.unwrappedById[id])
                                : static_cast<int32_t>(rawPosition);

  state.previousRawById[id] = rawPosition;
  state.unwrappedById[id] = unwrapped;
  state.hasPreviousById[id] = true;

  const int32_t bounded = (unwrapped > 32767) ? 32767 : ((unwrapped < -32767) ? -32767 : unwrapped);
  const int16_t boundedPosition = static_cast<int16_t>(bounded);
  const bool changed = !state.hasLastSentPositionById[id] ||
                       state.lastSentPositionById[id] != boundedPosition;
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
    uint8_t filterId) {
  const uint8_t sampleCount = parseMirrorSamples(
      servoBusService.lastTelemetryText(),
      state.mirrorSamples,
      static_cast<uint8_t>(sizeof(state.mirrorSamples) / sizeof(state.mirrorSamples[0])));

  uint8_t batchCount = 0U;
  for (uint8_t i = 0U; i < sampleCount; ++i) {
    tryAppendBatchItem(state, state.mirrorSamples[i].id, state.mirrorSamples[i].position, batchCount, filterId);
  }
  return batchCount;
}

void sendBatch(
    ILeaderPresenceService &presenceService,
    uint8_t count,
    TeleopMirrorState &state,
    uint16_t &requestCounter) {
  if (count == 0U) {
    return;
  }

  ++requestCounter;
  presenceService.requestTeleopMirrorBatch(
      state.batchIds,
      state.batchPositions,
      count,
      config::leader::kTeleopContinuousSpeedPct,
      requestCounter);
}

} // namespace

void LeaderTeleopMirrorTask::runLoop(
    ServoBusService &servoBusService,
    ILeaderPresenceService &presenceService,
    const std::atomic<bool> &continuousEnabled,
    const std::atomic<uint8_t> &servoIdFilter,
    const std::atomic<uint8_t> &runtimeMode,
    uint16_t &requestCounter) {
  static TeleopMirrorState state{};

  while (true) {
    if (!canMirrorNow(continuousEnabled, runtimeMode, presenceService)) {
      resetHistory(state);
      vTaskDelay(pdMS_TO_TICKS(config::leader::kTeleopMirrorTaskIdleDelayMs));
      continue;
    }

    refreshFollowerIds(state, presenceService);
    const uint8_t filterId = servoIdFilter.load();
    const uint8_t batchCount = buildMirrorBatch(state, servoBusService, filterId);
    sendBatch(presenceService, batchCount, state, requestCounter);

    vTaskDelay(pdMS_TO_TICKS(config::leader::kTeleopMirrorTaskActiveDelayMs));
  }
}

} // namespace soarm
