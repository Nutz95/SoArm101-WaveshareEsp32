#include "leader_app.h"

#include "../common/controller/controller_operation_profile.h"
#include "../common/servo/servo_control_opcode.h"
#include "leader_presence_service.h"

#include <cstring>

namespace soarm {

void LeaderApp::notifyFollowerLoadFeedbackUplink(bool enable) {
  if (!presenceService_->isFollowerLinked()) {
    return;
  }

  const uint16_t requestId = static_cast<uint16_t>(teleopContinuousRequestCounter_ + 1U);
  teleopContinuousRequestCounter_ = requestId;
  (void)presenceService_->requestServoControl(
      static_cast<uint8_t>(ServoControlOpcode::TeleopLoadFeedbackUplink),
      enable ? 1U : 0U,
      requestId);
}

void LeaderApp::pollTeleopLoadFeedback(uint32_t nowMs) {
  const ControllerOperationProfile profile =
      sanitizeControllerOperationProfile(controllerOperationProfile_.load());
  if (!isEspNowTeleopFeedbackProfile(profile)) {
    return;
  }

  auto *presence = static_cast<LeaderPresenceService *>(presenceService_.get());
  if (presence == nullptr) {
    return;
  }

  uint8_t loads[6]{};
  uint16_t requestId = 0U;
  bool received = false;
  while (presence->takeTeleopLoadFeedbackRx(loads, requestId)) {
    memcpy(teleopFeedbackLoads_, loads, sizeof(teleopFeedbackLoads_));
    received = true;
    (void)requestId;
  }
  if (!received) {
    return;
  }

  uint32_t windowStartMs = teleopFeedbackHzWindowStartMs_.load();
  if (windowStartMs == 0U) {
    teleopFeedbackHzWindowStartMs_.store(nowMs);
    windowStartMs = nowMs;
  }

  const uint16_t windowCount = static_cast<uint16_t>(teleopFeedbackHzWindowCount_.load() + 1U);
  teleopFeedbackHzWindowCount_.store(windowCount);

  const uint32_t windowMs = nowMs - windowStartMs;
  if (windowMs < 1000U) {
    return;
  }

  uint32_t hz = (static_cast<uint32_t>(windowCount) * 1000U) / windowMs;
  if (hz > 255U) {
    hz = 255U;
  }

  const uint8_t previousEwma = teleopFeedbackHzEwma_.load();
  const uint8_t nextEwma =
      previousEwma == 0U ? static_cast<uint8_t>(hz)
                         : static_cast<uint8_t>((previousEwma * 3U + hz) / 4U);
  teleopFeedbackHzEwma_.store(nextEwma);
  teleopFeedbackHzWindowStartMs_.store(nowMs);
  teleopFeedbackHzWindowCount_.store(0U);
}

} // namespace soarm
