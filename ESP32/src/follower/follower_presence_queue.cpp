#include "follower_presence_service.h"

#include "../Config/follower_runtime_config.h"

namespace soarm {

bool FollowerPresenceService::enqueueServoControl(uint8_t op, uint32_t value, uint16_t requestId, uint8_t sequence) {
  if (controlQueueCount_ >= config::follower::kServoControlQueueCapacity) {
    return false;
  }

  PendingServoControl &slot = controlQueue_[controlQueueTail_];
  slot.op = op;
  slot.value = value;
  slot.requestId = requestId;
  slot.sequence = sequence;

  controlQueueTail_ = static_cast<uint8_t>((controlQueueTail_ + 1U) % config::follower::kServoControlQueueCapacity);
  controlQueueCount_ = static_cast<uint8_t>(controlQueueCount_ + 1U);
  return true;
}

bool FollowerPresenceService::dequeueServoControl(uint8_t &op, uint32_t &value, uint16_t &requestId, uint8_t &sequence) {
  if (controlQueueCount_ == 0U) {
    return false;
  }

  const PendingServoControl &slot = controlQueue_[controlQueueHead_];
  op = slot.op;
  value = slot.value;
  requestId = slot.requestId;
  sequence = slot.sequence;

  controlQueueHead_ = static_cast<uint8_t>((controlQueueHead_ + 1U) % config::follower::kServoControlQueueCapacity);
  controlQueueCount_ = static_cast<uint8_t>(controlQueueCount_ - 1U);
  return true;
}

bool FollowerPresenceService::isDuplicateControlFrame(uint8_t op, uint32_t value, uint16_t requestId, uint8_t sequence) const {
  if (hasLastProcessedControl_ &&
      lastProcessedOp_ == op &&
      lastProcessedValue_ == value &&
      lastProcessedRequestId_ == requestId &&
      lastProcessedSequence_ == sequence) {
    return true;
  }

  for (uint8_t idx = 0U; idx < controlQueueCount_; ++idx) {
    const uint8_t ringIndex = static_cast<uint8_t>((controlQueueHead_ + idx) % config::follower::kServoControlQueueCapacity);
    const PendingServoControl &slot = controlQueue_[ringIndex];
    if (slot.op == op && slot.value == value && slot.requestId == requestId && slot.sequence == sequence) {
      return true;
    }
  }

  return false;
}

} // namespace soarm
