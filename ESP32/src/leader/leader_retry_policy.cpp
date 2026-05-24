#include "leader_retry_policy.h"

namespace soarm {

uint32_t computeFollowerAckDeadlineMs(
    uint32_t nowMs,
    uint32_t baseTimeoutMs,
    uint8_t retryCount,
    uint32_t retryIntervalMs,
    uint32_t slackMs) {
  const uint32_t retryBudgetMs = static_cast<uint32_t>(retryCount) * retryIntervalMs;
  return nowMs + baseTimeoutMs + retryBudgetMs + slackMs;
}

} // namespace soarm
