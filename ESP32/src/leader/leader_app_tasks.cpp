#include "leader_app.h"

#include "leader_servo_telemetry_task.h"
#include "leader_teleop_mirror_task.h"
#include "../Config/leader_runtime_config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace soarm {

void LeaderApp::startBackgroundTasks() {
  if (telemetryPollTaskHandle_ == nullptr) {
    TaskHandle_t taskHandle = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        &LeaderApp::telemetryPollTaskEntry,
        "servo_poll",
        4096,
        this,
        1,
        &taskHandle,
        1);
    if (created == pdPASS) {
      telemetryPollTaskHandle_ = taskHandle;
    }
  }

  if (teleopMirrorTaskHandle_ == nullptr) {
    TaskHandle_t taskHandle = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        &LeaderApp::teleopMirrorTaskEntry,
        "teleop_mirror",
        6144,
        this,
        1,
        &taskHandle,
        1);
    if (created == pdPASS) {
      teleopMirrorTaskHandle_ = taskHandle;
    }
  }
}

void LeaderApp::telemetryPollTaskEntry(void *context) {
  if (context != nullptr) {
    LeaderApp *app = static_cast<LeaderApp *>(context);
    LeaderServoTelemetryTask::runLoop(
        app->servoBusService_,
        app->teleopContinuousEnabled_,
        app->runtimeModeForTasks_);
  }
  vTaskDelete(nullptr);
}

void LeaderApp::teleopMirrorTaskEntry(void *context) {
  if (context != nullptr) {
    LeaderApp *app = static_cast<LeaderApp *>(context);
    LeaderTeleopMirrorTask::runLoop(
        app->servoBusService_,
        *app->presenceService_,
        app->teleopContinuousEnabled_,
        app->teleopContinuousServoIdFilter_,
        app->runtimeModeForTasks_,
        app->teleopContinuousRequestCounter_);
  }
  vTaskDelete(nullptr);
}

} // namespace soarm
