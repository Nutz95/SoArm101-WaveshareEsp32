#include <unity.h>

#include "leader/leader_retry_policy.h"

using soarm::computeFollowerAckDeadlineMs;

void setUp() {}
void tearDown() {}

void test_deadline_includes_six_retries_and_slack() {
  constexpr uint32_t nowMs = 1000U;
  constexpr uint32_t baseTimeoutMs = 150U;
  constexpr uint8_t retries = 6U;
  constexpr uint32_t retryIntervalMs = 25U;
  constexpr uint32_t slackMs = 20U;

  const uint32_t deadline = computeFollowerAckDeadlineMs(nowMs, baseTimeoutMs, retries, retryIntervalMs, slackMs);
  const uint32_t expected = nowMs + baseTimeoutMs + (static_cast<uint32_t>(retries) * retryIntervalMs) + slackMs;

  TEST_ASSERT_EQUAL_UINT32(expected, deadline);
}

void test_deadline_with_zero_retries_keeps_base_timeout_plus_slack() {
  constexpr uint32_t nowMs = 42U;
  constexpr uint32_t baseTimeoutMs = 100U;
  constexpr uint8_t retries = 0U;
  constexpr uint32_t retryIntervalMs = 10U;
  constexpr uint32_t slackMs = 5U;

  const uint32_t deadline = computeFollowerAckDeadlineMs(nowMs, baseTimeoutMs, retries, retryIntervalMs, slackMs);

  TEST_ASSERT_EQUAL_UINT32(nowMs + baseTimeoutMs + slackMs, deadline);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_deadline_includes_six_retries_and_slack);
  RUN_TEST(test_deadline_with_zero_retries_keeps_base_timeout_plus_slack);
  return UNITY_END();
}
