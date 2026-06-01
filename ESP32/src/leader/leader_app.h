#pragma once

#include "../common/app_state_machine.h"
#include "../common/interfaces/i_leader_presence_service.h"
#include "../common/status_led_service.h"
#include "../common/wifi_ota_service.h"
#include "../common/command/command_ack_status.h"
#include "../common/servo/servo_control_opcode.h"
#include "../common/nvs_calibration_store.h"
#include "../common/cpu_load_service.h"
#include "../common/servo/servo_bus_service.h"
#include "oled_presenter.h"
#include "oled_menu_controller.h"
#include "leader_calibration_oled_workflow.h"
#include "oled_display_config.h"
#include "leader_telemetry_state.h"
#include "leader_telemetry_stream_server.h"
#include "leader_usb_debug_service.h"
#include "leader_command_processor.h"
#include "leader_teleop_mirror_task.h"
#include "leader_teleop_pc_serial_bridge.h"
#include "leader_servo_passthrough.h"
#include "leader_xbox_controller_service.h"
#include "../common/teleop/teleop_transport_mode.h"
#include "../common/controller/controller_operation_profile.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace soarm {

class LeaderApp {
public:
  // Construct leader runtime services and initialize default state.
  LeaderApp();
  // Initialize hardware/services and start background tasks.
  void begin();
  // Execute one leader control-loop tick.
  void tick();

private:
  // Command handling extracted from tick()
  void handlePairingCommands();
    void handleResetPairingCommand(uint16_t requestId);
    void handleServoScanCommand(uint32_t scanTarget, uint16_t requestId);
    uint8_t commandCodeForScanTarget(uint32_t scanTarget) const;
    void buildScanStatusLine(bool runLeader, bool runFollower, bool followerSent, uint8_t localCount);
  void handleServoCommands();
  bool handleFollowerDebugCommands();
  bool handleLeaderDebugCommands();
  bool handleServoValueCommands();
    bool handleFollowerDebugCommand(
      uint16_t requestId,
      ServoControlOpcode op,
      LeaderCommandAction action,
      const char *statusText);
    bool handleLeaderDebugCommand(
      uint16_t requestId,
      bool enable,
      LeaderCommandAction action,
      const char *statusText);
    bool handleServoMoveValueCommand();
    bool handleServoSetIdValueCommand();
    bool handleServoSetModeValueCommand();
    bool handleTeleopMirrorValueCommand();
    bool handleTeleopContinuousValueCommand();
    bool handleTeleopTransportValueCommand();
    bool handleXboxModeCycleButtonSetValueCommand();
    bool handleTeleopCalibrationCaptureValueCommand();
  void handleServoMoveCommand(uint32_t value, uint16_t requestId);
  void handleServoSetIdCommand(uint32_t value, uint16_t requestId);
  void handleServoSetModeCommand(uint32_t value, uint16_t requestId);
  void handleTeleopMirrorCommand(uint32_t value, uint16_t requestId);
  void handleTeleopContinuousCommand(uint32_t value, uint16_t requestId);
  void handleTeleopTransportCommand(uint32_t value, uint16_t requestId);
  void handleXboxModeCycleButtonSetCommand(uint32_t value, uint16_t requestId);
  void handleTeleopCalibrationCaptureCommand(uint32_t value, uint16_t requestId);
  void rejectCalibrationCommandNotStarted();
  void handleCalibrationConfirmCenterCommand();
  void handleCalibrationFinishCommand();
  void handleCalibrationCancelCommand();
  void handleCalibrationSampleCommand(uint32_t value);
  ArmRole activeCalibrationRole() const;
  bool applyCalibrationCenter();
  void enterCalibrationRangePhase(ArmRole role);
  bool sampleCalibrationRangeCapture();
  bool commitCalibrationRangeCapture();
  void cancelCalibrationRangeCapture();
  void releaseCalibrationTorqueForActiveRole();
  void handleControllerModeCycleEvents();
  void handleModeCycleProfileStep();
  void handlePassthroughButtons(bool confirmPressed, bool validatePressed);
  void handleCalibrationButtons(
      ControllerOperationProfile profile,
      bool confirmPressed,
      bool validatePressed,
      uint32_t nowMs);
  void handleCalibrationPreviewButtons(bool confirmPressed, bool validatePressed);
  void handleCalibrationActiveButtons(
      ControllerOperationProfile profile,
      bool confirmPressed,
      bool validatePressed,
      uint32_t nowMs);
  void handleTeleopMirrorButtons(bool confirmPressed, bool validatePressed);
  void updateProfileSwitchStatus(ControllerOperationProfile profile);
  void applyControllerOperationProfile(uint8_t profile);
  void applyProfileExitTransitions(
      ControllerOperationProfile previous,
      ControllerOperationProfile next,
      bool leavingEngagedCalibration,
      bool switchingCalibrationPreview);
  void applyProfileCalibrationState(
      ControllerOperationProfile previous,
      ControllerOperationProfile next,
      bool switchingCalibrationPreview);
  void applyProfileTransportMode(ControllerOperationProfile profile);
  void applyProfileStatusTransition(
      ControllerOperationProfile previous,
      ControllerOperationProfile next);
  void engagePassthroughMode();
  void disengagePassthroughMode(ControllerOperationProfile fallbackProfile);
  void engageCalibrationMode();
  void disengageCalibrationMode(bool restoreCapturedRange);
  void nudgeFollowerLinkAfterCalibration();
  void syncWifiRadioPolicyForProfile(ControllerOperationProfile profile);
  void pollFollowerCalibrationCenterAck(uint32_t nowMs);
  void setTransientStatus(const char *text, uint32_t holdMs);
  void beginCommandTracking(uint16_t requestId, uint8_t commandCode);
  void setLeaderCommandStatus(CommandAckStatus status);
  void setFollowerCommandStatus(CommandAckStatus status);
  void awaitFollowerAck(uint16_t requestId, uint8_t op, uint32_t timeoutMs);
  void setFollowerRetryPayload(uint8_t op, uint32_t value, uint8_t maxRetries);
  void updateFollowerAckTracking(uint32_t nowMs);
  void runStartupServoScans(uint32_t nowMs);
  void updateServoHealthFlags();

  // State computation extracted from tick()
  void updateLocalInputs(uint32_t uptimeMs);
  void computeModeAndStatus();
  bool applyOtaModeAndStatus(ControllerOperationProfile profile);
  bool applyPassthroughModeAndStatus(ControllerOperationProfile profile);
  bool applyHeldCommandModeAndStatus(
      ControllerOperationProfile profile,
      bool calibrationProfileSelected,
      bool calibrationProfileActive);
  bool applyServoFaultModeAndStatus();
  void applyRuntimeModeAndStatus(
      ControllerOperationProfile profile,
      bool followerIpValid,
      bool rangeCaptureActive,
      bool calibrationProfileSelected);
  void setCalibrationPreviewStatus(ControllerOperationProfile profile);
  void updateFollowerState();
  void renderStatusLeds();

  // Telemetry / display extracted from tick()
  void buildTelemetrySnapshot(LeaderTelemetrySnapshot &snapshot, uint32_t uptimeMs);
  void fillLeaderMirrorPositions(LeaderTelemetrySnapshot &snapshot);
  void refreshOled(uint32_t uptimeMs);
  void runDeferredBootStages(uint32_t uptimeMs);
  void tickCoreServices(uint32_t uptimeMs);
  void tickCommandHandlers(bool passthroughActive);
  void tickControlState(uint32_t uptimeMs, bool passthroughActive);
  void tickTelemetrySnapshot(uint32_t uptimeMs, bool passthroughActive);

  void startBackgroundTasks();
  static void telemetryPollTaskEntry(void *context);
  static void teleopMirrorTaskEntry(void *context);

  ArmStateMachine     stateMachine_;
  NvsCalibrationStore calibrationStore_;
  CalibrationProfile leaderCalibrationProfile_{};
  CalibrationProfile followerCalibrationProfile_{};
  CalibrationProfile leaderCalibrationProfileBackup_{};
  CalibrationProfile followerCalibrationProfileBackup_{};
  CalibrationProfile leaderCalibrationWorkingProfile_{};
  CalibrationProfile followerCalibrationWorkingProfile_{};
  StatusLedService    statusLedService_;
  WifiOtaService      wifiOta_;
  CpuLoadService      cpuLoadService_;
  ServoBusService     servoBusService_;
  bool                servoDebugManual_{false};
  std::unique_ptr<ILeaderPresenceService> presenceService_;
  OledDisplayConfig   oledConfig_;
  OledPresenter       oled_;
  OledMenuController  oledMenu_;
  LeaderCalibrationOledWorkflow calibrationOledWorkflow_;
  LeaderTelemetryState telemetryState_;
  LeaderTelemetryStreamServer telemetryStreamServer_;
  LeaderUsbDebugService usbDebugService_{telemetryStreamServer_};
  ArmStateInputs      localInputs_;
  ArmRuntimeState     followerState_;
  OperationMode       mode_;
  char                followerIpHint_[16];
  char                statusLine_[24];
  uint32_t            commandStatusHoldUntilMs_{0U};
  uint16_t            commandRequestId_{0U};
  uint8_t             commandCode_{0U};
  CommandAckStatus    leaderCommandStatus_{CommandAckStatus::None};
  CommandAckStatus    followerCommandStatus_{CommandAckStatus::None};
  bool                followerAckPending_{false};
  uint16_t            followerAckRequestId_{0U};
  uint8_t             followerAckCommandOp_{0U};
  uint32_t            followerAckDeadlineMs_{0U};
  bool                followerRetryEnabled_{false};
  uint8_t             followerRetryOp_{0U};
  uint32_t            followerRetryValue_{0U};
  uint8_t             followerRetryRemaining_{0U};
  uint32_t            followerNextRetryMs_{0U};
  uint32_t            followerAckSentAtMs_{0U};
  uint8_t             followerAckRetriesUsed_{0U};
  uint8_t             followerAckLastRttMs_{0U};
  uint8_t             followerAckTimeoutCount_{0U};
  std::atomic<bool>   teleopContinuousEnabled_{false};
  std::atomic<uint8_t> teleopContinuousServoIdFilter_{0U};
  std::atomic<uint8_t> teleopContinuousSpeedPct_{35U};
  std::atomic<uint8_t> teleopTransportMode_{static_cast<uint8_t>(TeleopTransportMode::EspNow)};
  std::atomic<uint8_t> controllerOperationProfile_{toProfileRaw(ControllerOperationProfile::TeleopEspNow)};
  std::atomic<uint8_t> calibrationPhase_{0U};
  std::atomic<uint8_t> runtimeModeForTasks_{0U};
  std::atomic<bool> passthroughEngaged_{false};
  std::atomic<bool> calibrationEngaged_{false};
  std::atomic<bool> followerCalibrationCenterPending_{false};
  uint16_t followerCalibrationCenterRequestId_{0U};
  uint32_t followerCalibrationCenterStartedMs_{0U};
  uint32_t calibrationCenterConfirmArmedAtMs_{0U};
  TeleopMirrorLatencyMetrics teleopMirrorLatencyMetrics_{};
  LeaderTeleopWifiBridge teleopWifiBridge_{};
  LeaderTeleopPcSerialBridge teleopPcSerialBridge_{};
  LeaderServoPassthrough servoPassthrough_{};
  LeaderXboxControllerService xboxControllerService_{};
  uint16_t            teleopContinuousRequestCounter_{40000U};
  void               *telemetryPollTaskHandle_{nullptr};
  void               *teleopMirrorTaskHandle_{nullptr};
  bool                leaderStartupScanDone_{false};
  bool                followerStartupScanDone_{false};
  bool                followerStartupScanPending_{false};
  uint16_t            followerStartupScanRequestId_{60000U};
  uint32_t            followerStartupScanDeadlineMs_{0U};
  uint32_t            followerStartupScanRetryMs_{0U};
  bool                leaderServoFault_{false};
  bool                followerServoFault_{false};
  uint32_t            lastTelemetrySnapshotMs_{0U};
  uint32_t            lastOledRefreshMs_{0U};
  uint32_t            bootMs_{0U};
  bool                deferredOledReady_{false};
  bool                deferredServoBusReady_{false};
  bool                deferredNetworkReady_{false};
  bool                deferredBackgroundTasksReady_{false};
  bool                deferredBleReady_{false};
  bool                deferredBootComplete_{false};
};

} // namespace soarm
