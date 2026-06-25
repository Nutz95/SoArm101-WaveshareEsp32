#pragma once

#include "leader_calibration_oled_workflow.h"
#include "oled_menu/oled_menu_render_output.h"
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
  void showOtaAwaitEnter(const char *routerIp);
  void showOtaActive(const char *routerIp);
  void showNavigationMenu(const OledMenuRenderOutput &output);
  void showFeedbackTeleop(const int8_t loads[6], uint8_t feedbackHz, uint8_t mirrorLoopMs);

private:
  OledPresenter &presenter_;
};

} // namespace soarm
