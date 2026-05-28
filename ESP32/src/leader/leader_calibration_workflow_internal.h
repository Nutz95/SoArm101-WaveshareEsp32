#pragma once

#include "../common/types/calibration_profile.h"

#include <cstdint>

namespace soarm {

// Pure calibration workflow helpers. Separated so they can be unit-tested on
// a native host without pulling in Arduino/FreeRTOS dependencies.

// Reset a working profile to the sentinel that indicates no range has been
// captured yet: minPosition = 4095, maxPosition = 0 for every servo slot.
void resetWorkingProfile(CalibrationProfile &profile);

// Walk a telemetry text of the form "#1 p2048 t25;#2 p1500 t24;" and expand
// the per-servo extrema in 'profile'. Returns true when at least one servo
// position was parsed.
bool expandWorkingProfileFromTelemetry(CalibrationProfile &profile, const char *telemetryText);

// Return true when at least one servo slot in 'profile' has maxPosition >=
// minPosition (i.e. a real range was captured and is ready to save).
bool profileHasRange(const CalibrationProfile &profile);

} // namespace soarm
