#pragma once

#include "../common/app_state_machine.h"
#include "../common/espnow_presence_service.h"
#include "../common/nvs_calibration_store.h"
#include "../common/status_led_service.h"
#include "../common/wifi_ota_service.h"

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
  EspNowPresenceService espNowPresence_;
  ArmStateInputs      localInputs_;
};

} // namespace soarm
