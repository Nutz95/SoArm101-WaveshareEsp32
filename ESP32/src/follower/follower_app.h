#pragma once

#include "../common/app_state_machine.h"
#include "../common/command/command_ack_status.h"
#include "../common/interfaces/i_follower_presence_service.h"
#include "../common/nvs_calibration_store.h"
#include "../common/status_led_service.h"
#include "../common/wifi_ota_service.h"
#include "../common/servo/servo_bus_service.h"
#include "follower_teleop_wifi_bridge.h"

#include <memory>

namespace soarm {

class FollowerApp {
public:
  FollowerApp();
  void begin();
  void tick();

private:
  void processIncomingTeleopBatch();
  void processIncomingTeleopWifiBatch();
  void processIncomingServoControl();
  void processIncomingServoScan();
  void publishServoTelemetry();
  void updateStateAndLeds(uint32_t uptimeMs);
  CommandAckStatus executeServoControl(uint8_t op, uint32_t value);
  CommandAckStatus handleDebugEnable(uint32_t value);
  CommandAckStatus handleDebugDisable(uint32_t value);
  CommandAckStatus handleMove(uint32_t value);
  CommandAckStatus handleTeleopMirror(uint32_t value);
  CommandAckStatus handleSetId(uint32_t value);
  CommandAckStatus handleSetMode(uint32_t value);
  CommandAckStatus handleCalibrationCapture(uint32_t value);
  CommandAckStatus handleCenterAll(uint32_t value);

  ArmStateMachine     stateMachine_;
  NvsCalibrationStore calibrationStore_;
  CalibrationProfile  calibrationProfile_{};
  StatusLedService    statusLedService_;
  WifiOtaService      wifiOta_;
  ServoBusService     servoBusService_;
  FollowerTeleopWifiBridge teleopWifiBridge_;
  std::unique_ptr<IFollowerPresenceService> presenceService_;
  ArmStateInputs      localInputs_;
};

} // namespace soarm
