#pragma once

#include "../common/controller/controller_operation_profile.h"
#include "../common/types/arm_role.h"
#include "../common/types/calibration_profile.h"

#include <cstdint>

namespace soarm {

enum class CalibrationOledScreen : uint8_t {
  Inactive = 0,
  AwaitEnter,
  ArmPrompt,
  Centering,
  RangeTable,
  ResultBanner,
};

struct CalibrationOledInput {
  ControllerOperationProfile profile{ControllerOperationProfile::TeleopEspNow};
  uint8_t calibrationPhase{0U};
  bool calibrationEngaged{false};
  bool followerCenterPending{false};
  ArmRole activeRole{ArmRole::Leader};
  const CalibrationProfile *workingProfile{nullptr};
  const char *liveTelemetry{nullptr};
  uint32_t nowMs{0U};
  uint32_t centerConfirmArmedAtMs{0U};
};

class LeaderCalibrationOledWorkflow {
public:
  CalibrationOledScreen resolve(const CalibrationOledInput &input) const;
  const char *resultBannerText(uint32_t nowMs) const;

  void showCommittedResult(ArmRole role, uint32_t nowMs);
  void showCanceledResult(ArmRole role, uint32_t nowMs);
  void clearResultBanner();

private:
  static constexpr uint32_t kResultBannerMs = 2500U;

  uint32_t resultUntilMs_{0U};
  char resultText_[28]{};
};

} // namespace soarm
