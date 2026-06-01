#include "leader_calibration_oled_workflow.h"

#include "../common/controller/controller_operation_profile.h"

#include <cstdio>
#include <cstring>

namespace soarm {

namespace {

bool isCalibrationProfile(ControllerOperationProfile profile) {
  return profile == ControllerOperationProfile::CalibrationLeader ||
         profile == ControllerOperationProfile::CalibrationFollower;
}

const char *armLabel(ArmRole role) {
  return role == ArmRole::Follower ? "Follower" : "Leader";
}

} // namespace

CalibrationOledScreen LeaderCalibrationOledWorkflow::resolve(const CalibrationOledInput &input) const {
  if (input.nowMs < resultUntilMs_ && resultText_[0] != '\0') {
    return CalibrationOledScreen::ResultBanner;
  }

  if (!isCalibrationProfile(input.profile)) {
    return CalibrationOledScreen::Inactive;
  }

  if (!input.calibrationEngaged) {
    return CalibrationOledScreen::AwaitEnter;
  }

  if (input.followerCenterPending) {
    return CalibrationOledScreen::Centering;
  }

  if (input.calibrationPhase == 0U) {
    return CalibrationOledScreen::ArmPrompt;
  }

  return CalibrationOledScreen::RangeTable;
}

const char *LeaderCalibrationOledWorkflow::resultBannerText(uint32_t nowMs) const {
  if (nowMs >= resultUntilMs_ || resultText_[0] == '\0') {
    return nullptr;
  }
  return resultText_;
}

void LeaderCalibrationOledWorkflow::showCommittedResult(ArmRole role, uint32_t nowMs) {
  snprintf(resultText_, sizeof(resultText_), "Cal %s OK", armLabel(role));
  resultUntilMs_ = nowMs + kResultBannerMs;
}

void LeaderCalibrationOledWorkflow::showCanceledResult(ArmRole role, uint32_t nowMs) {
  snprintf(resultText_, sizeof(resultText_), "Cal %s canceled", armLabel(role));
  resultUntilMs_ = nowMs + kResultBannerMs;
}

void LeaderCalibrationOledWorkflow::clearResultBanner() {
  resultUntilMs_ = 0U;
  resultText_[0] = '\0';
}

} // namespace soarm
