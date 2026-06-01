#pragma once

#include "leader_calibration_oled_workflow.h"
#include "oled_presenter.h"

#include <cstdint>

namespace soarm {

// Routes leader OLED rendering between the standard dashboard and calibration layouts.
class OledMenuController {
public:
  explicit OledMenuController(OledPresenter &presenter);

  void showDashboard(const char *leaderIp,
                     const char *followerIp,
                     OperationMode mode,
                     TeleopTransportMode transportMode,
                     const char *status,
                     uint32_t nowMs);

  void showCalibration(CalibrationOledScreen screen,
                       const CalibrationOledInput &input,
                       const char *resultText);

  void showWifiDirectAwaitEnter(const char *leaderRouterIp, const char *followerRouterIp);
  void showWifiDirectWaitingFollower(const char *leaderApIp);
  void showWifiDirectAwaitStart(const char *leaderApIp, const char *followerApIp);

private:
  OledPresenter &presenter_;
};

} // namespace soarm
