#pragma once

#include "../common/app_state_machine.h"
#include "../common/interfaces/i_follower_presence_service.h"
#include "../common/nvs_calibration_store.h"
#include "../common/status_led_service.h"
#include "../common/wifi_ota_service.h"

#include <memory>

namespace soarm {

class FollowerApp {
public:
  FollowerApp();
  void begin();
  void tick();

private:
  ArmStateMachine     stateMachine_;
  NvsCalibrationStore calibrationStore_;
  StatusLedService    statusLedService_;
  WifiOtaService      wifiOta_;
  std::unique_ptr<IFollowerPresenceService> presenceService_;
  ArmStateInputs      localInputs_;
};

} // namespace soarm
