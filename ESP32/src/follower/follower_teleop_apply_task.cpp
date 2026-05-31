#include "follower_teleop_apply_task.h"

#include "follower_app.h"

#include "../Config/common_runtime_config.h"
#include "../Config/follower_runtime_config.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace soarm {

namespace {

void copyFrame(
    FollowerTeleopApplyTask::TeleopFrame &dest,
    const uint8_t *ids,
    const int16_t *positions,
    uint8_t count,
    uint8_t speedPercent,
    uint16_t requestId,
    uint8_t flags) {
  dest.count = count;
  dest.speedPercent = speedPercent;
  dest.requestId = requestId;
  dest.flags = flags;
  for (uint8_t i = 0U; i < config::common::kTeleopBatchMaxServos; ++i) {
    dest.ids[i] = (i < count && ids != nullptr) ? ids[i] : 0U;
    dest.positions[i] = (i < count && positions != nullptr) ? positions[i] : 0;
  }
  dest.valid = count > 0U;
}

} // namespace

bool FollowerTeleopApplyTask::ingestLatestWifi(FollowerApp &app, TeleopFrame &out) {
  uint8_t ids[config::common::kTeleopBatchMaxServos]{};
  int16_t positions[config::common::kTeleopBatchMaxServos]{};
  uint8_t count = 0U;
  uint8_t speedPercent = 0U;
  uint16_t requestId = 0U;
  uint8_t flags = 0U;
  if (!app.teleopWifiBridge_.drainLatestBatch(
          ids, positions, config::common::kTeleopBatchMaxServos, count, speedPercent, requestId, flags)) {
    return false;
  }
  copyFrame(out, ids, positions, count, speedPercent, requestId, flags);
  return true;
}

bool FollowerTeleopApplyTask::ingestLatestPcSerial(FollowerApp &app, TeleopFrame &out) {
  uint8_t ids[config::common::kTeleopBatchMaxServos]{};
  int16_t positions[config::common::kTeleopBatchMaxServos]{};
  uint8_t count = 0U;
  uint8_t speedPercent = 0U;
  uint16_t requestId = 0U;
  uint8_t flags = 0U;
  if (!app.teleopPcSerialBridge_.drainLatestBatch(
          ids, positions, config::common::kTeleopBatchMaxServos, count, speedPercent, requestId, flags)) {
    return false;
  }
  copyFrame(out, ids, positions, count, speedPercent, requestId, flags);
  return true;
}

bool FollowerTeleopApplyTask::ingestLatestEspNow(FollowerApp &app, TeleopFrame &out) {
  uint8_t ids[config::common::kTeleopBatchMaxServos]{};
  int16_t positions[config::common::kTeleopBatchMaxServos]{};
  uint8_t count = 0U;
  uint8_t speedPct = 0U;
  uint16_t requestId = 0U;
  if (!app.presenceService_->consumeTeleopMirrorBatch(
          ids, positions, config::common::kTeleopBatchMaxServos, count, speedPct, requestId)) {
    return false;
  }
  copyFrame(out, ids, positions, count, speedPct, requestId, 0U);
  return true;
}

const FollowerTeleopApplyTask::TeleopFrame *FollowerTeleopApplyTask::selectFrame(
    const TeleopFrame &pc,
    const TeleopFrame &wifi,
    const TeleopFrame &espNow) {
  if (pc.valid) {
    return &pc;
  }
  if (wifi.valid) {
    return &wifi;
  }
  if (espNow.valid) {
    return &espNow;
  }
  return nullptr;
}

void FollowerTeleopApplyTask::runLoop(FollowerApp &app) {
  while (true) {
    TeleopFrame pcFrame{};
    TeleopFrame wifiFrame{};
    TeleopFrame espNowFrame{};
    const bool gotPc = ingestLatestPcSerial(app, pcFrame);
    const bool gotWifi = ingestLatestWifi(app, wifiFrame);
    const bool gotEspNow = ingestLatestEspNow(app, espNowFrame);

    const TeleopFrame *selected = selectFrame(pcFrame, wifiFrame, espNowFrame);
    if (selected != nullptr) {
      app.markTeleopActivity(millis());
      const bool fromEspNow = selected == &espNowFrame;
      (void)app.applyTeleopBatch(
          selected->ids,
          selected->positions,
          selected->count,
          selected->speedPercent,
          selected->requestId,
          selected->flags,
          fromEspNow);
      vTaskDelay(pdMS_TO_TICKS(config::follower::kTeleopApplyTaskPeriodMs));
      continue;
    }

    if (gotPc || gotWifi || gotEspNow) {
      app.markTeleopActivity(millis());
    }

    vTaskDelay(pdMS_TO_TICKS(config::follower::kTeleopApplyTaskIdleDelayMs));
  }
}

} // namespace soarm
