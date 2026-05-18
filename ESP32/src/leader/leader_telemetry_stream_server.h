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

private:
  void handleIncomingCommands();
  void handleAction(LeaderCommandAction action);
  void streamTelemetryFrame();

  LeaderTelemetryState &telemetryState_;
  WiFiServer server_;
  WiFiClient client_;
  LeaderTelemetrySerializer serializer_;
  LeaderCommandProcessor commandProcessor_;
  bool started_{false};
  bool streamEnabled_{false};
  uint32_t lastStreamMs_{0};
};

} // namespace soarm
