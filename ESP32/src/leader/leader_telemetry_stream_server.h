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
  bool consumeResetPairingRequested();
  bool consumeServoScanRequested();
  bool consumeServoDebugEnableRequested();
  bool consumeServoDebugDisableRequested();
  bool consumeServoMoveRequested(uint32_t &value);
  bool consumeServoSetIdRequested(uint32_t &value);
  bool consumeServoSetModeRequested(uint32_t &value);

private:
  void handleIncomingCommands();
  void handleAction(LeaderCommandAction action, uint32_t value);
  void streamTelemetryFrame();

  LeaderTelemetryState &telemetryState_;
  WiFiServer server_;
  WiFiClient client_;
  LeaderTelemetrySerializer serializer_;
  LeaderCommandProcessor commandProcessor_;
  bool started_{false};
  bool streamEnabled_{false};
  bool resetPairingRequested_{false};
  bool servoScanRequested_{false};
  bool servoDebugEnableRequested_{false};
  bool servoDebugDisableRequested_{false};
  bool servoMoveRequested_{false};
  bool servoSetIdRequested_{false};
  bool servoSetModeRequested_{false};
  uint32_t servoMoveValue_{0U};
  uint32_t servoSetIdValue_{0U};
  uint32_t servoSetModeValue_{0U};
  uint32_t lastStreamMs_{0};
};

} // namespace soarm
