#include "follower_teleop_load_sampler_task.h"

#include "follower_app.h"
#include "follower_presence_service.h"

#include "../Config/common_runtime_config.h"
#include "../Config/follower_runtime_config.h"
#include "../common/teleop/teleop_load_feedback_codec.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace soarm {

void FollowerTeleopLoadSamplerTask::runLoop(FollowerApp &app) {
  vTaskDelay(pdMS_TO_TICKS(config::follower::kTeleopLoadSamplerPhaseOffsetMs));

  int16_t rawLoads[config::common::kTeleopBatchMaxServos]{};
  uint8_t wireLoads[config::common::kTeleopBatchMaxServos]{};

  while (true) {
    const uint32_t loopStartMs = millis();
    auto *presence = static_cast<FollowerPresenceService *>(app.presenceService_.get());
    if (presence != nullptr && presence->isTeleopLoadFeedbackUplinkEnabled()) {
      const uint8_t readCount =
          app.servoBusService_.syncReadPresentLoad(rawLoads, config::common::kTeleopBatchMaxServos);
      if (readCount > 0U) {
        for (uint8_t i = 0U; i < config::common::kTeleopBatchMaxServos; ++i) {
          wireLoads[i] = teleop_load_feedback::encodeLoadWire(rawLoads[i]);
        }
        app.teleopLoadSnapshot_.publish(wireLoads, loopStartMs);
      } else {
        app.teleopLoadSnapshot_.noteSamplerSkip();
      }
    }

    const uint32_t elapsedMs = millis() - loopStartMs;
    const uint32_t periodMs = config::follower::kTeleopLoadSamplerPeriodMs;
    const uint32_t delayMs =
        elapsedMs >= periodMs ? config::follower::kTeleopApplyTaskIdleDelayMs : (periodMs - elapsedMs);
    vTaskDelay(pdMS_TO_TICKS(delayMs));
  }
}

} // namespace soarm
