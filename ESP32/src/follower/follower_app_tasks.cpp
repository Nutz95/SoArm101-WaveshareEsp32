#include "follower_app.h"

#include "follower_teleop_apply_task.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace soarm {

void FollowerApp::startBackgroundTasks() {
  if (teleopApplyTaskHandle_ != nullptr) {
    return;
  }

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

void FollowerApp::teleopApplyTaskEntry(void *context) {
  if (context != nullptr) {
    FollowerApp *app = static_cast<FollowerApp *>(context);
    FollowerTeleopApplyTask::runLoop(*app);
  }
  vTaskDelete(nullptr);
}

} // namespace soarm
