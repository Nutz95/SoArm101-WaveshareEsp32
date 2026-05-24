#include <unity.h>

#include "leader/leader_servo_command_policy.h"

using soarm::ServoSetIdRoutingDecision;
using soarm::decideServoSetIdRouting;

void test_set_id_route_disabled_when_both_debug_disabled() {
  const ServoSetIdRoutingDecision d = decideServoSetIdRouting(false, false);
  TEST_ASSERT_FALSE(d.executeLeaderLocal);
  TEST_ASSERT_FALSE(d.forwardFollower);
}

void test_set_id_route_enabled_for_both_when_both_debug_enabled() {
  const ServoSetIdRoutingDecision d = decideServoSetIdRouting(true, true);
  TEST_ASSERT_TRUE(d.executeLeaderLocal);
  TEST_ASSERT_TRUE(d.forwardFollower);
}

void test_set_id_route_forwards_follower_when_leader_debug_off_follower_on() {
  const ServoSetIdRoutingDecision d = decideServoSetIdRouting(false, true);
  TEST_ASSERT_FALSE(d.executeLeaderLocal);
  TEST_ASSERT_TRUE(d.forwardFollower);
}

void test_set_id_route_forwards_follower_when_leader_debug_on_follower_off() {
  const ServoSetIdRoutingDecision d = decideServoSetIdRouting(true, false);
  TEST_ASSERT_TRUE(d.executeLeaderLocal);
  TEST_ASSERT_TRUE(d.forwardFollower);
}

void setUp() {
}

void tearDown() {
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_set_id_route_disabled_when_both_debug_disabled);
  RUN_TEST(test_set_id_route_enabled_for_both_when_both_debug_enabled);
  RUN_TEST(test_set_id_route_forwards_follower_when_leader_debug_off_follower_on);
  RUN_TEST(test_set_id_route_forwards_follower_when_leader_debug_on_follower_off);
  return UNITY_END();
}
