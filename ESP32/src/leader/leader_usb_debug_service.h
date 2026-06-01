#pragma once

#include "leader_command_processor.h"
#include "leader_telemetry_stream_server.h"

namespace soarm {

// Phase 3: same dashboard command/snapshot protocol as Wi-Fi :9090, over USB CDC Serial.
class LeaderUsbDebugService {
public:
  explicit LeaderUsbDebugService(LeaderTelemetryStreamServer &telemetryStream);

  void tick();

private:
  void drainIncomingCommands();
  void streamTelemetryIfEnabled();

  LeaderTelemetryStreamServer &telemetryStream_;
  uint32_t lastStreamMs_{0U};
};

} // namespace soarm
