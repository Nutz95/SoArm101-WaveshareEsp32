#pragma once

#include "../common/app_state_machine.h"
#include "../common/interfaces/i_leader_presence_service.h"
#include "../common/status_led_service.h"
#include "../common/wifi_ota_service.h"
#include "../common/nvs_calibration_store.h"
#include "../common/cpu_load_service.h"
#include "oled_presenter.h"
#include "oled_display_config.h"
#include "leader_telemetry_state.h"
#include "leader_telemetry_stream_server.h"

#include <cstdint>
#include <memory>

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
  CpuLoadService      cpuLoadService_;
  std::unique_ptr<ILeaderPresenceService> presenceService_;
  OledDisplayConfig   oledConfig_;
  OledPresenter       oled_;
  LeaderTelemetryState telemetryState_;
  LeaderTelemetryStreamServer telemetryStreamServer_;
  ArmStateInputs      localInputs_;
  ArmRuntimeState     followerState_;
  OperationMode       mode_;
  char                followerIpHint_[16];
  char                statusLine_[24];
  uint32_t            lastOledRefreshMs_;
};

} // namespace soarm
