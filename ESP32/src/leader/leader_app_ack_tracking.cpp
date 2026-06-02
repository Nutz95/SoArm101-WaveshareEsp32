#include "leader_app.h"

#include "leader_retry_policy.h"
#include "../Config/leader_runtime_config.h"

#include <Arduino.h>
#include <cstring>

namespace soarm {

void LeaderApp::beginCommandTracking(uint16_t requestId, uint8_t commandCode) {
  commandRequestId_ = requestId;
  commandCode_ = commandCode;
  leaderCommandStatus_ = CommandAckStatus::Accepted;
  followerCommandStatus_ = CommandAckStatus::Accepted;
  followerAckPending_ = false;
  followerRetryEnabled_ = false;
  followerRetryRemaining_ = 0U;
}

void LeaderApp::setLeaderCommandStatus(CommandAckStatus status) {
  leaderCommandStatus_ = status;
}

void LeaderApp::setFollowerCommandStatus(CommandAckStatus status) {
  followerCommandStatus_ = status;
}

void LeaderApp::awaitFollowerAck(uint16_t requestId, uint8_t op, uint32_t timeoutMs) {
  followerAckPending_ = true;
  followerAckRequestId_ = requestId;
  followerAckCommandOp_ = op;
  const uint32_t nowMs = millis();
  const uint8_t retryCount = followerRetryEnabled_ ? followerRetryRemaining_ : 0U;
  followerAckDeadlineMs_ = computeFollowerAckDeadlineMs(
      nowMs,
      timeoutMs,
      retryCount,
      config::leader::kFollowerRetryIntervalMs,
      config::leader::kFollowerAckDeadlineSlackMs);
  followerAckSentAtMs_ = nowMs;
  followerAckRetriesUsed_ = 0U;
  followerAckLastRttMs_ = 0U;
  followerNextRetryMs_ = nowMs + config::leader::kFollowerRetryIntervalMs;
}

void LeaderApp::setFollowerRetryPayload(uint8_t op, uint32_t value, uint8_t maxRetries) {
  followerRetryEnabled_ = true;
  followerRetryOp_ = op;
  followerRetryValue_ = value;
  followerRetryRemaining_ = maxRetries;
}

void LeaderApp::updateFollowerAckTracking(uint32_t nowMs) {
  if (!followerAckPending_) {
    return;
  }

  const bool requestIdMatch = presenceService_->followerLastAckRequestId() == followerAckRequestId_;
  const bool opMatch = presenceService_->followerLastAckCommandOp() == followerAckCommandOp_;
  if (requestIdMatch && opMatch) {
    followerCommandStatus_ = static_cast<CommandAckStatus>(presenceService_->followerLastAckStatus());
    const uint32_t rttMs = nowMs - followerAckSentAtMs_;
    followerAckLastRttMs_ = static_cast<uint8_t>(rttMs > config::leader::kFollowerAckRttClampMs
                                                     ? config::leader::kFollowerAckRttClampMs
                                                     : rttMs);
    followerAckPending_ = false;
    followerRetryEnabled_ = false;
    clearDeferHomeStaReconnectIfDone();
    return;
  }

  if (followerRetryEnabled_ && followerRetryRemaining_ > 0U && nowMs >= followerNextRetryMs_) {
    const bool resent = presenceService_->requestServoControl(
        followerRetryOp_,
        followerRetryValue_,
        followerAckRequestId_);
    followerRetryRemaining_ = static_cast<uint8_t>(followerRetryRemaining_ - 1U);
    followerNextRetryMs_ = nowMs + config::leader::kFollowerRetryIntervalMs;
    if (resent) {
      followerAckRetriesUsed_ = static_cast<uint8_t>(followerAckRetriesUsed_ + 1U);
    }
    if (!resent && followerRetryRemaining_ == 0U) {
      followerCommandStatus_ = CommandAckStatus::Failed;
      followerAckLastRttMs_ = 0U;
      followerAckPending_ = false;
      followerRetryEnabled_ = false;
      return;
    }
  }

  if (nowMs >= followerAckDeadlineMs_) {
    followerCommandStatus_ = CommandAckStatus::Timeout;
    followerAckLastRttMs_ = 0U;
    if (followerAckTimeoutCount_ < 255U) {
      followerAckTimeoutCount_ = static_cast<uint8_t>(followerAckTimeoutCount_ + 1U);
    }
    followerAckPending_ = false;
    followerRetryEnabled_ = false;
    clearDeferHomeStaReconnectIfDone();
  }
}

void LeaderApp::setTransientStatus(const char *text, uint32_t holdMs) {
  strncpy(statusLine_, text, sizeof(statusLine_) - 1);
  statusLine_[sizeof(statusLine_) - 1] = '\0';
  commandStatusHoldUntilMs_ = millis() + holdMs;
}

} // namespace soarm
