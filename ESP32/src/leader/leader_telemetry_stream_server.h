#pragma once

#include "leader_command_processor.h"
#include "leader_telemetry_serializer.h"
#include "leader_telemetry_state.h"

#include <WiFi.h>

namespace soarm {

class LeaderTelemetryStreamServer {
public:
  explicit LeaderTelemetryStreamServer(LeaderTelemetryState &telemetryState);

  bool begin(uint16_t port);
  void tick();
  bool consumeResetPairingRequested(uint16_t &requestId);
  bool consumeServoScanRequested(uint32_t &value, uint16_t &requestId);
  bool consumeServoDebugEnableRequested(uint16_t &requestId);
  bool consumeServoDebugDisableRequested(uint16_t &requestId);
  bool consumeServoDebugEnableFollowerRequested(uint16_t &requestId);
  bool consumeServoDebugDisableFollowerRequested(uint16_t &requestId);
  bool consumeServoMoveRequested(uint32_t &value, uint16_t &requestId);
  bool consumeServoSetIdRequested(uint32_t &value, uint16_t &requestId);
  bool consumeServoSetModeRequested(uint32_t &value, uint16_t &requestId);
  bool consumeTeleopMirrorRequested(uint32_t &value, uint16_t &requestId);

private:
  void handleIncomingCommands();
  void handleAction(LeaderCommandAction action, uint32_t value, uint16_t requestId);
  void streamTelemetryFrame();

  LeaderTelemetryState &telemetryState_;
  WiFiServer server_;
  WiFiClient client_;
  LeaderTelemetrySerializer serializer_;
  LeaderCommandProcessor commandProcessor_;
  bool started_{false};
  bool streamEnabled_{false};
  bool resetPairingRequested_{false};
  uint16_t resetPairingRequestId_{0U};
  bool servoScanRequested_{false};
  uint32_t servoScanValue_{0U};
  uint16_t servoScanRequestId_{0U};
  bool servoDebugEnableRequested_{false};
  uint16_t servoDebugEnableRequestId_{0U};
  bool servoDebugDisableRequested_{false};
  uint16_t servoDebugDisableRequestId_{0U};
  bool servoDebugEnableFollowerRequested_{false};
  uint16_t servoDebugEnableFollowerRequestId_{0U};
  bool servoDebugDisableFollowerRequested_{false};
  uint16_t servoDebugDisableFollowerRequestId_{0U};
  bool servoMoveRequested_{false};
  uint16_t servoMoveRequestId_{0U};
  bool servoSetIdRequested_{false};
  uint16_t servoSetIdRequestId_{0U};
  bool servoSetModeRequested_{false};
  uint16_t servoSetModeRequestId_{0U};
  bool teleopMirrorRequested_{false};
  uint16_t teleopMirrorRequestId_{0U};
  uint32_t servoMoveValue_{0U};
  uint32_t servoSetIdValue_{0U};
  uint32_t servoSetModeValue_{0U};
  uint32_t teleopMirrorValue_{0U};
  uint32_t lastStreamMs_{0};
};

} // namespace soarm
