#pragma once

#include "../types/calibration_profile.h"

#include <cstdint>

namespace soarm {

bool updateCalibrationProfileFromTelemetry(
    CalibrationProfile &profile,
    const char *telemetryText,
    bool captureMin);

int16_t remapServoPositionWithCalibration(
    uint8_t servoId,
    int16_t sourcePosition,
    const CalibrationProfile &sourceProfile,
    const CalibrationProfile &targetProfile);

} // namespace soarm