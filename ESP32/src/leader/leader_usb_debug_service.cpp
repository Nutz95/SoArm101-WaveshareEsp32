#include "leader_usb_debug_service.h"

#include "../Config/leader_runtime_config.h"

#include <Arduino.h>

namespace soarm {

LeaderUsbDebugService::LeaderUsbDebugService(LeaderTelemetryStreamServer &telemetryStream)
    : telemetryStream_(telemetryStream) {}

void LeaderUsbDebugService::tick() {
  drainIncomingCommands();
  streamTelemetryIfEnabled();
}

void LeaderUsbDebugService::drainIncomingCommands() {
  constexpr size_t kFrameSize = sizeof(LeaderCommandProcessor::CommandFrame);

  while (Serial.available() >= static_cast<int>(kFrameSize)) {
    LeaderCommandProcessor::CommandFrame frame{};
    const size_t readLen =
        Serial.readBytes(reinterpret_cast<char *>(&frame), static_cast<size_t>(kFrameSize));
    if (readLen != kFrameSize) {
      return;
    }

    telemetryStream_.ingestDashboardCommand(frame);
  }
}

void LeaderUsbDebugService::streamTelemetryIfEnabled() {
  if (!telemetryStream_.isTelemetryStreamEnabled()) {
    return;
  }

  const uint32_t nowMs = millis();
  if ((nowMs - lastStreamMs_) < config::leader::kTelemetryStreamPeriodMs) {
    return;
  }

  lastStreamMs_ = nowMs;
  const LeaderTelemetrySerializer::Packet packet = telemetryStream_.buildTelemetryPacket();
  Serial.write(reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

} // namespace soarm
