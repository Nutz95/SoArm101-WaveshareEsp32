#include "calibration_profile_utils.h"

#include <cstdio>

namespace soarm {

namespace {

constexpr uint16_t kServoPositionMin = 0U;
constexpr uint16_t kServoPositionMax = 4095U;

uint16_t clampServoPosition(int position) {
  if (position < static_cast<int>(kServoPositionMin)) {
    return kServoPositionMin;
  }

  if (position > static_cast<int>(kServoPositionMax)) {
    return kServoPositionMax;
  }

  return static_cast<uint16_t>(position);
}

bool validServoId(unsigned int servoId) {
  return servoId >= 1U && servoId <= CalibrationProfile::kServoCount;
}

} // namespace

bool updateCalibrationProfileFromTelemetry(
    CalibrationProfile &profile,
    const char *telemetryText,
    bool captureMin) {
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
    if (sscanf(cursor, "#%u p%d", &servoId, &position) == 2 && validServoId(servoId)) {
      const uint8_t servoIndex = static_cast<uint8_t>(servoId - 1U);
      const uint16_t clamped = clampServoPosition(position);
      if (captureMin) {
        profile.minPosition[servoIndex] = clamped;
        if (profile.maxPosition[servoIndex] < clamped) {
          profile.maxPosition[servoIndex] = clamped;
        }
      } else {
        profile.maxPosition[servoIndex] = clamped;
        if (profile.minPosition[servoIndex] > clamped) {
          profile.minPosition[servoIndex] = clamped;
        }
      }
      updated = true;
    }

    while (*cursor != '\0' && *cursor != ';' && *cursor != '#') {
      ++cursor;
    }
    if (*cursor == ';') {
      ++cursor;
    }
  }

  return updated;
}

int16_t remapServoPositionWithCalibration(
    uint8_t servoId,
    int16_t sourcePosition,
    const CalibrationProfile &sourceProfile,
    const CalibrationProfile &targetProfile) {
  if (servoId == 0U || servoId > CalibrationProfile::kServoCount) {
    return sourcePosition;
  }

  const uint8_t servoIndex = static_cast<uint8_t>(servoId - 1U);
  const uint16_t sourceMin = sourceProfile.minPosition[servoIndex];
  const uint16_t sourceMax = sourceProfile.maxPosition[servoIndex];
  const uint16_t targetMin = targetProfile.minPosition[servoIndex];
  const uint16_t targetMax = targetProfile.maxPosition[servoIndex];

  if (sourceMax <= sourceMin || targetMax <= targetMin) {
    return sourcePosition;
  }

  const float numerator = static_cast<float>(sourcePosition) - static_cast<float>(sourceMin);
  const float denominator = static_cast<float>(sourceMax - sourceMin);
  float normalized = numerator / denominator;
  if (normalized < 0.0f) {
    normalized = 0.0f;
  }
  if (normalized > 1.0f) {
    normalized = 1.0f;
  }

  const float mapped = static_cast<float>(targetMin) +
                       normalized * static_cast<float>(targetMax - targetMin);
  return static_cast<int16_t>(mapped + 0.5f);
}

} // namespace soarm