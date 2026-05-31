#pragma once

#include "../common/app_state_machine.h"
#include "../common/command/command_ack_status.h"
#include "../common/interfaces/i_follower_presence_service.h"
#include "../common/nvs_calibration_store.h"
#include "../common/status_led_service.h"
#include "../common/wifi_ota_service.h"
#include "../common/servo/servo_bus_service.h"
#include "follower_teleop_wifi_bridge.h"
#include "follower_teleop_pc_serial_bridge.h"

#include <memory>

namespace soarm {

class FollowerTeleopApplyTask;

class FollowerApp {
  friend class FollowerTeleopApplyTask;

public:
  FollowerApp();
  void begin();
  void tick();

  bool applyTeleopBatch(
      const uint8_t *ids,
      const int16_t *positions,
      uint8_t count,
      uint8_t speedPercent,
      uint16_t requestId,
      uint8_t flags,
      bool stageEspNowAck);
  void markTeleopActivity(uint32_t nowMs);
  bool isTeleopBusPaused(uint32_t nowMs) const;

private:
  void startBackgroundTasks();
  static void teleopApplyTaskEntry(void *context);

  bool ensureTeleopServosReady(const uint8_t *ids, uint8_t count);
  bool applyOneTeleopWifiBatch(
      uint8_t *ids,
      int16_t *positions,
      uint8_t count,
      uint8_t speedPercent,
      uint16_t requestId,
      uint8_t flags);
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
  CommandAckStatus handleCalibrationCenter(uint32_t value);

  ArmStateMachine     stateMachine_;
  NvsCalibrationStore calibrationStore_;
  CalibrationProfile  calibrationProfile_{};
  StatusLedService    statusLedService_;
  WifiOtaService      wifiOta_;
  ServoBusService     servoBusService_;
  FollowerTeleopWifiBridge teleopWifiBridge_;
  FollowerTeleopPcSerialBridge teleopPcSerialBridge_;
  std::unique_ptr<IFollowerPresenceService> presenceService_;
  ArmStateInputs      localInputs_;
  uint32_t            lastServoTelemetryPublishMs_{0U};
  uint32_t            lastTeleopActivityMs_{0U};
  bool                teleopPreparedById_[256]{};
  TaskHandle_t        teleopApplyTaskHandle_{nullptr};
};

} // namespace soarm
