#include "leader_servo_telemetry_task.h"

#include "../Config/leader_runtime_config.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace soarm {

void LeaderServoTelemetryTask::runLoop(
    ServoBusService &servoBusService,
    const std::atomic<bool> &continuousEnabled,
    const std::atomic<uint8_t> &runtimeMode) {
  uint32_t lastDiscoveryScanMs = 0U;

  while (true) {
    const bool active = continuousEnabled.load();
    const OperationMode mode = static_cast<OperationMode>(runtimeMode.load());
    const uint32_t nowMs = millis();

    const bool allowDiscoveryScan = mode != OperationMode::Teleoperation &&
                                    mode != OperationMode::CalibrationLeader &&
                                    mode != OperationMode::CalibrationFollower;

    if (allowDiscoveryScan &&
        ((nowMs - lastDiscoveryScanMs) >= config::leader::kFollowerScanRetryIntervalMs ||
         servoBusService.lastScanCount() == 0U ||
         servoBusService.lastIdsText()[0] == '-')) {
      servoBusService.scan();
      lastDiscoveryScanMs = nowMs;
    } else {
      servoBusService.refreshKnownTelemetryFast();
    }

    const uint32_t delayMs = active ? config::leader::kServoTelemetryTaskActiveDelayMs
                                    : config::leader::kServoTelemetryTaskIdleDelayMs;
    vTaskDelay(pdMS_TO_TICKS(delayMs));
  }
}

} // namespace soarm
