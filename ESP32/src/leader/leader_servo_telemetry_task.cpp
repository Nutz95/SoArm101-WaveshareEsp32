#include "leader_servo_telemetry_task.h"

#include "../Config/leader_runtime_config.h"
#include "../common/controller/controller_operation_profile.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace soarm {

void LeaderServoTelemetryTask::runLoop(
    ServoBusService &servoBusService,
    const std::atomic<bool> &continuousEnabled,
    const std::atomic<uint8_t> &runtimeMode,
    const std::atomic<uint8_t> &controllerOperationProfile) {
  uint32_t lastDiscoveryScanMs = 0U;

  while (true) {
    const bool active = continuousEnabled.load();
    const OperationMode mode = static_cast<OperationMode>(runtimeMode.load());
    const ControllerOperationProfile profile =
        sanitizeControllerOperationProfile(controllerOperationProfile.load());
    const uint32_t nowMs = millis();

    const bool calibrationMode =
        mode == OperationMode::CalibrationLeader || mode == OperationMode::CalibrationFollower;

    if (mode == OperationMode::Passthrough) {
      vTaskDelay(pdMS_TO_TICKS(50U));
      continue;
    }

    if (calibrationMode) {
      servoBusService.refreshKnownTelemetryFast();
      vTaskDelay(pdMS_TO_TICKS(config::leader::kServoTelemetryTaskCalibrationDelayMs));
      continue;
    }

    const bool allowDiscoveryScan =
        mode != OperationMode::Teleoperation && !continuousEnabled.load();

    if (allowDiscoveryScan &&
        ((nowMs - lastDiscoveryScanMs) >= config::leader::kFollowerScanRetryIntervalMs ||
         servoBusService.lastScanCount() == 0U ||
         servoBusService.lastIdsText()[0] == '-')) {
      servoBusService.scan();
      lastDiscoveryScanMs = nowMs;
    } else {
      servoBusService.refreshKnownTelemetryFast();
    }

    uint32_t delayMs = config::leader::kServoTelemetryTaskIdleDelayMs;
    if (active) {
      delayMs = usesEspNowTurboDownlinkProfile(profile) ? config::leader::kServoTelemetryTaskTurboActiveDelayMs
                                                        : config::leader::kServoTelemetryTaskActiveDelayMs;
    }
    vTaskDelay(pdMS_TO_TICKS(delayMs));
  }
}

} // namespace soarm
