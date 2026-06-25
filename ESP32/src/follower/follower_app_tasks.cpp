#include "follower_app.h"

#include "follower_teleop_apply_task.h"
#include "follower_teleop_load_sampler_task.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace soarm {

void FollowerApp::startBackgroundTasks() {
  if (teleopApplyTaskHandle_ == nullptr) {
    TaskHandle_t taskHandle = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        &FollowerApp::teleopApplyTaskEntry,
        "teleop_apply",
        6144,
        this,
        1,
        &taskHandle,
        1);
    if (created == pdPASS) {
      teleopApplyTaskHandle_ = taskHandle;
    }
  }

  if (teleopLoadSamplerTaskHandle_ == nullptr) {
    TaskHandle_t samplerHandle = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        &FollowerApp::teleopLoadSamplerTaskEntry,
        "teleop_load",
        4096,
        this,
        1,
        &samplerHandle,
        1);
    if (created == pdPASS) {
      teleopLoadSamplerTaskHandle_ = samplerHandle;
    }
  }
}

void FollowerApp::teleopLoadSamplerTaskEntry(void *context) {
  if (context != nullptr) {
    FollowerApp *app = static_cast<FollowerApp *>(context);
    FollowerTeleopLoadSamplerTask::runLoop(*app);
  }
  vTaskDelete(nullptr);
}

void FollowerApp::teleopApplyTaskEntry(void *context) {
  if (context != nullptr) {
    FollowerApp *app = static_cast<FollowerApp *>(context);
    FollowerTeleopApplyTask::runLoop(*app);
  }
  vTaskDelete(nullptr);
}

} // namespace soarm
