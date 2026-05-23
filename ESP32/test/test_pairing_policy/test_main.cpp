#include <unity.h>

#include "common/pairing/pairing_policy.h"

using soarm::PairingPolicy;

void test_leader_accepts_pair_request_when_unpaired() {
  TEST_ASSERT_TRUE(PairingPolicy::shouldAcceptLeaderPairRequest(false, false));
}

void test_leader_accepts_pair_request_from_known_peer_when_paired() {
  TEST_ASSERT_TRUE(PairingPolicy::shouldAcceptLeaderPairRequest(true, true));
}

void test_leader_rejects_pair_request_from_unknown_peer_when_paired() {
  TEST_ASSERT_FALSE(PairingPolicy::shouldAcceptLeaderPairRequest(true, false));
}

void test_follower_accepts_pair_ack_when_unpaired() {
  TEST_ASSERT_TRUE(PairingPolicy::shouldAcceptFollowerPairAck(false, false));
}

void test_follower_accepts_pair_ack_from_same_leader() {
  TEST_ASSERT_TRUE(PairingPolicy::shouldAcceptFollowerPairAck(true, true));
}

void test_follower_rejects_pair_ack_from_different_leader() {
  TEST_ASSERT_FALSE(PairingPolicy::shouldAcceptFollowerPairAck(true, false));
}

void setUp() {
}

void tearDown() {
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_leader_accepts_pair_request_when_unpaired);
  RUN_TEST(test_leader_accepts_pair_request_from_known_peer_when_paired);
  RUN_TEST(test_leader_rejects_pair_request_from_unknown_peer_when_paired);
  RUN_TEST(test_follower_accepts_pair_ack_when_unpaired);
  RUN_TEST(test_follower_accepts_pair_ack_from_same_leader);
  RUN_TEST(test_follower_rejects_pair_ack_from_different_leader);
  return UNITY_END();
}
