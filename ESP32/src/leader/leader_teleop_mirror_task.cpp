#include "leader_teleop_mirror_task.h"

#include "../Config/leader_runtime_config.h"
#include "../common/servo/servo_control_opcode.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>

namespace soarm {

namespace {

struct MirrorSample {
  uint8_t id;
  int16_t position;
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

void parseIdList(const char *idsText, bool present[256]) {
  for (uint16_t i = 0; i < 256U; ++i) {
    present[i] = false;
  }

  if (idsText == nullptr) {
    return;
  }

  const char *cursor = idsText;
  while (*cursor != '\0') {
    unsigned int id = 0U;
    if (sscanf(cursor, "%u", &id) == 1 && id < 256U) {
      present[id] = true;
    }
    while (*cursor != '\0' && *cursor != ',') {
      ++cursor;
    }
    if (*cursor == ',') {
      ++cursor;
    }
  }
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

} // namespace

void LeaderTeleopMirrorTask::runLoop(
    ServoBusService &servoBusService,
    ILeaderPresenceService &presenceService,
    const std::atomic<bool> &continuousEnabled,
    const std::atomic<uint8_t> &servoIdFilter,
    uint16_t &requestCounter) {
  MirrorSample samples[16]{};
  bool followerIds[256]{};
  int16_t previousRawById[256]{};
  int32_t unwrappedById[256]{};
  bool hasPreviousById[256]{};

  while (true) {
    const bool active = continuousEnabled.load();
    if (!active || !presenceService.isFollowerLinked()) {
      for (uint16_t i = 0; i < 256U; ++i) {
        hasPreviousById[i] = false;
      }
      vTaskDelay(pdMS_TO_TICKS(config::leader::kTeleopMirrorTaskIdleDelayMs));
      continue;
    }

    parseIdList(presenceService.followerServoIds(), followerIds);
    const uint8_t sampleCount = parseMirrorSamples(
        servoBusService.lastTelemetryText(),
        samples,
        static_cast<uint8_t>(sizeof(samples) / sizeof(samples[0])));
    const uint8_t filterId = servoIdFilter.load();

    for (uint8_t i = 0U; i < sampleCount; ++i) {
      const uint8_t id = samples[i].id;
      if (id == 0U || !followerIds[id] || (filterId != 0U && id != filterId)) {
        continue;
      }

      const int16_t rawPosition = samples[i].position;
      const int32_t unwrapped = hasPreviousById[id]
                                    ? unwrapPosition(rawPosition, previousRawById[id], unwrappedById[id])
                                    : static_cast<int32_t>(rawPosition);

      previousRawById[id] = rawPosition;
      unwrappedById[id] = unwrapped;
      hasPreviousById[id] = true;

      const int32_t bounded = (unwrapped > 32767) ? 32767 : ((unwrapped < -32767) ? -32767 : unwrapped);
      const int16_t target = static_cast<int16_t>(bounded);
      const uint32_t packed = (static_cast<uint32_t>(id) & 0xFFU)
                              | ((static_cast<uint32_t>(static_cast<uint16_t>(target)) & 0xFFFFU) << 8)
                              | ((static_cast<uint32_t>(config::leader::kTeleopContinuousSpeedPct) & 0xFFU) << 24);

      ++requestCounter;
      presenceService.requestServoControl(
          static_cast<uint8_t>(ServoControlOpcode::TeleopMirror),
          packed,
          requestCounter);
    }

    vTaskDelay(pdMS_TO_TICKS(config::leader::kTeleopMirrorTaskActiveDelayMs));
  }
}

} // namespace soarm
