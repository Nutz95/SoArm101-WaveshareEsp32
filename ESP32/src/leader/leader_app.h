#pragma once

#include "../common/app_state_machine.h"
#include "../common/nvs_calibration_store.h"
#include "../common/status_led_service.h"
#include "../common/wifi_ota_service.h"
#include "../common/espnow_presence_service.h"
#include "oled_presenter.h"
#include "oled_display_config.h"
#include "leader_web_telemetry.h"
#include <cstdint>

namespace soarm {

class LeaderApp {
public:
  LeaderApp();
  void begin();
  void tick();

private:
  ArmStateMachine     stateMachine_;
  NvsCalibrationStore calibrationStore_;
  StatusLedService    statusLedService_;
  WifiOtaService      wifiOta_;
  EspNowPresenceService espNowPresence_;
  OledDisplayConfig   oledConfig_;
  OledPresenter       oled_;
  LeaderWebTelemetry  webTelemetry_;
  ArmStateInputs      localInputs_;
  ArmRuntimeState     followerState_;
  OperationMode       mode_;
  char                followerIpHint_[16];
  char                statusLine_[24];
  uint32_t            lastOledRefreshMs_;
};

} // namespace soarm
