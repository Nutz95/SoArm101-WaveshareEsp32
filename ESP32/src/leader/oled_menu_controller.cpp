#include "oled_menu_controller.h"

#include <cstdio>
#include <cstring>

namespace soarm {

namespace {

int16_t readServoPositionFromTelemetry(const char *telemetryText, uint8_t servoId) {
  if (telemetryText == nullptr || telemetryText[0] == '\0' || telemetryText[0] == '-') {
    return -1;
  }

  char needle[12]{};
  snprintf(needle, sizeof(needle), "#%u p", static_cast<unsigned>(servoId));

  const char *found = strstr(telemetryText, needle);
  if (found == nullptr) {
    return -1;
  }

  unsigned int parsedId = 0U;
  int position = 0;
  if (sscanf(found, "#%u p%d", &parsedId, &position) != 2) {
    return -1;
  }

  return static_cast<int16_t>(position);
}

void formatServoPair(char *line,
                     size_t lineSize,
                     uint8_t servoA,
                     uint8_t servoB,
                     const CalibrationProfile *profile,
                     const char *telemetryText) {
  const auto formatOne = [&](uint8_t servoId, char *out, size_t outSize) {
    if (profile == nullptr || servoId < 1U || servoId > CalibrationProfile::kServoCount) {
      snprintf(out, outSize, "%u:---", static_cast<unsigned>(servoId));
      return;
    }

    const uint8_t index = static_cast<uint8_t>(servoId - 1U);
    const uint16_t minPos = profile->minPosition[index];
    const uint16_t maxPos = profile->maxPosition[index];
    const int16_t current = readServoPositionFromTelemetry(telemetryText, servoId);
    if (current < 0) {
      snprintf(out, outSize, "%u:--- %u-%u", static_cast<unsigned>(servoId), minPos, maxPos);
      return;
    }

    snprintf(
        out,
        outSize,
        "%u:%d %u-%u",
        static_cast<unsigned>(servoId),
        static_cast<int>(current),
        minPos,
        maxPos);
  };

  char left[14]{};
  char right[14]{};
  formatOne(servoA, left, sizeof(left));
  formatOne(servoB, right, sizeof(right));
  snprintf(line, lineSize, "%s %s", left, right);
}

} // namespace

OledMenuController::OledMenuController(OledPresenter &presenter) : presenter_(presenter) {}

void OledMenuController::showDashboard(const char *leaderIp,
                                       const char *followerIp,
                                       OperationMode mode,
                                       TeleopTransportMode transportMode,
                                       const char *status,
                                       uint32_t nowMs) {
  presenter_.showDashboard(leaderIp, followerIp, mode, transportMode, status, nowMs);
}

void OledMenuController::showCalibration(CalibrationOledScreen screen,
                                         const CalibrationOledInput &input,
                                         const char *resultText) {
  switch (screen) {
  case CalibrationOledScreen::ResultBanner:
    presenter_.showCalibrationResultBanner(resultText != nullptr ? resultText : "Cal done");
    return;
  case CalibrationOledScreen::AwaitEnter:
    presenter_.showCalibrationAwaitEnter(
        input.activeRole == ArmRole::Follower ? "Follower" : "Leader");
    return;
  case CalibrationOledScreen::ArmPrompt:
    presenter_.showCalibrationArmPrompt(
        input.activeRole == ArmRole::Follower ? "Follower" : "Leader",
        input.nowMs,
        input.centerConfirmArmedAtMs);
    return;
  case CalibrationOledScreen::Centering: {
    const char *arm = input.activeRole == ArmRole::Follower ? "Follower" : "Leader";
    presenter_.showCalibrationCentering(arm, "centering...");
    return;
  }
  case CalibrationOledScreen::RangeTable: {
    char line1[22]{};
    char line2[22]{};
    char line3[22]{};
    formatServoPair(line1, sizeof(line1), 1U, 2U, input.workingProfile, input.liveTelemetry);
    formatServoPair(line2, sizeof(line2), 3U, 4U, input.workingProfile, input.liveTelemetry);
    formatServoPair(line3, sizeof(line3), 5U, 6U, input.workingProfile, input.liveTelemetry);
    presenter_.showCalibrationRangeTable(line1, line2, line3, "A:Val B:Can");
    return;
  }
  case CalibrationOledScreen::Inactive:
  default:
    return;
  }
}

} // namespace soarm
