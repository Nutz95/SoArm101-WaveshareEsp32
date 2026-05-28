#include "leader_teleop_mirror_task_internal.h"

#include <cstdio>

namespace soarm {

namespace {

constexpr uint32_t kTeleopBatchAckTimeoutMs = 300U;

uint8_t clampMsToU8(uint32_t value) {
  return static_cast<uint8_t>(value > 255U ? 255U : value);
}

bool consumePendingBatch(TeleopMirrorState &state, uint16_t requestId, uint32_t &sentAtMs) {
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(state.pendingBatches) / sizeof(state.pendingBatches[0])); ++i) {
    PendingBatch &slot = state.pendingBatches[i];
    if (!slot.active || slot.requestId != requestId) {
      continue;
    }

    sentAtMs = slot.sentAtMs;
    slot.active = false;
    return true;
  }
  return false;
}

void appendLatencySample(TeleopMirrorState &state, uint8_t sampleMs) {
  state.latencySamples[state.latencySampleWriteIndex] = sampleMs;
  state.latencySampleWriteIndex = static_cast<uint8_t>((state.latencySampleWriteIndex + 1U) % (sizeof(state.latencySamples) / sizeof(state.latencySamples[0])));
  if (state.latencySampleCount < static_cast<uint8_t>(sizeof(state.latencySamples) / sizeof(state.latencySamples[0]))) {
    state.latencySampleCount = static_cast<uint8_t>(state.latencySampleCount + 1U);
  }
}

uint8_t computeP95LatencyMs(const TeleopMirrorState &state) {
  if (state.latencySampleCount == 0U) {
    return 0U;
  }

  uint8_t sorted[32]{};
  for (uint8_t i = 0U; i < state.latencySampleCount; ++i) {
    sorted[i] = state.latencySamples[i];
  }

  for (uint8_t i = 1U; i < state.latencySampleCount; ++i) {
    uint8_t value = sorted[i];
    int8_t j = static_cast<int8_t>(i - 1U);
    while (j >= 0 && sorted[j] > value) {
      sorted[j + 1] = sorted[j];
      --j;
    }
    sorted[j + 1] = value;
  }

  const uint8_t idx = static_cast<uint8_t>((static_cast<uint16_t>(state.latencySampleCount - 1U) * 95U) / 100U);
  return sorted[idx];
}

void updateLatencyFromAck(TeleopMirrorState &state, uint16_t ackRequestId, uint32_t nowMs, TeleopMirrorLatencyMetrics &latencyMetrics) {
  uint32_t sentAtMs = 0U;
  if (!consumePendingBatch(state, ackRequestId, sentAtMs)) {
    return;
  }

  const uint8_t latencyMs = clampMsToU8(nowMs - sentAtMs);
  latencyMetrics.lastMs.store(latencyMs);

  const uint8_t previousEwma = latencyMetrics.ewmaMs.load();
  const uint16_t blended = (previousEwma == 0U)
                               ? static_cast<uint16_t>(latencyMs)
                               : static_cast<uint16_t>((static_cast<uint16_t>(previousEwma) * 7U + static_cast<uint16_t>(latencyMs) + 4U) / 8U);
  latencyMetrics.ewmaMs.store(static_cast<uint8_t>(blended));

  appendLatencySample(state, latencyMs);
  latencyMetrics.p95Ms.store(computeP95LatencyMs(state));
}

} // namespace

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

void registerPendingBatch(TeleopMirrorState &state, uint16_t requestId, uint32_t nowMs) {
  PendingBatch &slot = state.pendingBatches[state.pendingWriteIndex];
  slot.requestId = requestId;
  slot.sentAtMs = nowMs;
  slot.active = true;
  state.pendingWriteIndex = static_cast<uint8_t>((state.pendingWriteIndex + 1U) % (sizeof(state.pendingBatches) / sizeof(state.pendingBatches[0])));
}

uint8_t countPendingBatches(const TeleopMirrorState &state) {
  uint8_t count = 0U;
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(state.pendingBatches) / sizeof(state.pendingBatches[0])); ++i) {
    if (state.pendingBatches[i].active) {
      ++count;
    }
  }
  return count;
}

void processFollowerBatchAck(TeleopMirrorState &state, ILeaderPresenceService &presenceService, TeleopMirrorLatencyMetrics &latencyMetrics, uint32_t nowMs) {
  const uint8_t ackCommandOp = presenceService.followerLastAckCommandOp();
  if (ackCommandOp != static_cast<uint8_t>(ServoControlOpcode::TeleopMirrorBatch)) {
    return;
  }

  const uint16_t ackRequestId = presenceService.followerLastAckRequestId();
  if (state.hasProcessedAck &&
      state.lastProcessedAckRequestId == ackRequestId &&
      state.lastProcessedAckCommandOp == ackCommandOp) {
    return;
  }

  state.lastProcessedAckRequestId = ackRequestId;
  state.lastProcessedAckCommandOp = ackCommandOp;
  state.hasProcessedAck = true;
  updateLatencyFromAck(state, ackRequestId, nowMs, latencyMetrics);
}

void processWifiBatchAck(TeleopMirrorState &state, LeaderTeleopWifiBridge &teleopWifiBridge, TeleopMirrorLatencyMetrics &latencyMetrics, uint32_t nowMs) {
  uint16_t ackRequestId = 0U;
  uint8_t ackStatus = 0U;
  if (!teleopWifiBridge.pollAck(ackRequestId, ackStatus)) {
    return;
  }

  (void)ackStatus;
  updateLatencyFromAck(state, ackRequestId, nowMs, latencyMetrics);
}

void expireOldPendingBatches(TeleopMirrorState &state, uint32_t nowMs, TeleopMirrorLatencyMetrics &latencyMetrics) {
  uint8_t timeoutCount = latencyMetrics.timeoutCount.load();
  for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(state.pendingBatches) / sizeof(state.pendingBatches[0])); ++i) {
    PendingBatch &slot = state.pendingBatches[i];
    if (!slot.active) {
      continue;
    }

    if ((nowMs - slot.sentAtMs) <= kTeleopBatchAckTimeoutMs) {
      continue;
    }

    slot.active = false;
    if (timeoutCount < 255U) {
      timeoutCount = static_cast<uint8_t>(timeoutCount + 1U);
    }
  }

  latencyMetrics.timeoutCount.store(timeoutCount);
  latencyMetrics.pendingCount.store(countPendingBatches(state));
}

} // namespace soarm