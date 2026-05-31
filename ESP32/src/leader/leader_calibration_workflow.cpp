#include "leader_calibration_workflow_internal.h"

#include <cstdio>
#include <cstring>

namespace soarm {

void resetWorkingProfile(CalibrationProfile &profile) {
  for (uint8_t servoIndex = 0U; servoIndex < CalibrationProfile::kServoCount; ++servoIndex) {
    profile.minPosition[servoIndex] = 4095U;
    profile.maxPosition[servoIndex] = 0U;
  }
}

bool expandWorkingProfileFromTelemetry(CalibrationProfile &profile, const char *telemetryText) {
  if (telemetryText == nullptr) {
    return false;
  }

  bool updated = false;
  const char *cursor = telemetryText;
  while (*cursor != '\0') {
    while (*cursor != '\0' && *cursor != '#') {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }

    unsigned int servoId = 0U;
    int position = 0;
    if (sscanf(cursor, "#%u p%d", &servoId, &position) == 2 &&
        servoId >= 1U && servoId <= CalibrationProfile::kServoCount) {
      const uint8_t servoIndex = static_cast<uint8_t>(servoId - 1U);
      uint16_t clamped = static_cast<uint16_t>(position);
      if (position < 0) {
        clamped = 0U;
      } else if (position > 4095) {
        clamped = 4095U;
      }
      if (clamped < profile.minPosition[servoIndex]) {
        profile.minPosition[servoIndex] = clamped;
      }
      if (clamped > profile.maxPosition[servoIndex]) {
        profile.maxPosition[servoIndex] = clamped;
      }
      updated = true;
    }

    ++cursor;

    while (*cursor != '\0' && *cursor != ';' && *cursor != '#') {
      ++cursor;
    }
    if (*cursor == ';') {
      ++cursor;
    }
  }

  return updated;
}

bool profileHasRange(const CalibrationProfile &profile) {
  constexpr uint16_t kMinSpanCounts = 50U;
  uint8_t validServoCount = 0U;
  for (uint8_t servoIndex = 0U; servoIndex < CalibrationProfile::kServoCount; ++servoIndex) {
    const uint16_t minPos = profile.minPosition[servoIndex];
    const uint16_t maxPos = profile.maxPosition[servoIndex];
    if (maxPos > static_cast<uint16_t>(minPos + kMinSpanCounts)) {
      ++validServoCount;
    }
  }
  return validServoCount >= 3U;
}

} // namespace soarm
