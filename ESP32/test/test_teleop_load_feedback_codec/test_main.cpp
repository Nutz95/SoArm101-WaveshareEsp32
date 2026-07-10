#include <unity.h>

#include "../../src/common/presence/presence_message_type.h"
#include "../../src/common/teleop/teleop_load_feedback_codec.h"

using namespace soarm;
using namespace soarm::teleop_load_feedback;

void test_encode_load_wire_clamps() {
  TEST_ASSERT_EQUAL_UINT8(0U, encodeLoadWire(-4));
  TEST_ASSERT_EQUAL_UINT8(0U, encodeLoadWire(4));
  TEST_ASSERT_EQUAL_UINT8(127U, encodeLoadWire(2000));
  TEST_ASSERT_EQUAL_UINT8(62U, encodeLoadWire(500));
  TEST_ASSERT_EQUAL_UINT8(62U, encodeLoadWire(-500));
}

void test_load_feedback_round_trip() {
  uint8_t loads[6] = {1U, 2U, 3U, 10U, 20U, 127U};
  constexpr uint16_t kGripperPos = 2048U;
  uint8_t buffer[kLoadFeedbackWireSize]{};
  size_t outLen = 0U;

  TEST_ASSERT_TRUE(encodePacket(42U, loads, kGripperPos, buffer, sizeof(buffer), outLen));
  TEST_ASSERT_EQUAL_size_t(kLoadFeedbackWireSize, outLen);
  TEST_ASSERT_TRUE(isTeleopLoadFeedbackPacket(buffer, outLen));

  uint16_t requestId = 0U;
  uint8_t decoded[6]{};
  uint16_t gripperPresentPos = 0U;
  TEST_ASSERT_TRUE(decodePacket(buffer, outLen, requestId, decoded, gripperPresentPos));
  TEST_ASSERT_EQUAL_UINT16(42U, requestId);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(loads, decoded, 6);
  TEST_ASSERT_EQUAL_UINT16(kGripperPos, gripperPresentPos);
}

void test_load_feedback_rejects_wrong_type() {
  uint8_t buffer[kLoadFeedbackWireSize]{};
  size_t outLen = 0U;
  uint8_t loads[6]{};
  TEST_ASSERT_TRUE(encodePacket(1U, loads, 0U, buffer, sizeof(buffer), outLen));
  buffer[2] = static_cast<uint8_t>(PresenceMessageType::TeleopMirrorCompact);
  TEST_ASSERT_FALSE(isTeleopLoadFeedbackPacket(buffer, outLen));
}

void test_gripper_baseline_subtracts_idle_offset() {
  uint8_t baseline = 0U;
  TEST_ASSERT_EQUAL_UINT8(0U, netGripperLoadWire(4U, baseline));
  TEST_ASSERT_EQUAL_UINT8(0U, netGripperLoadWire(4U, baseline));
  TEST_ASSERT_EQUAL_UINT8(6U, netGripperLoadWire(10U, baseline));
}

void test_gripper_baseline_does_not_absorb_contact() {
  uint8_t baseline = 4U;
  for (uint8_t i = 0U; i < 64U; ++i) {
    const uint8_t net = netGripperLoadWire(40U, baseline);
    TEST_ASSERT_GREATER_THAN_UINT8(20U, net);
  }
}

void setUp() {}

void tearDown() {}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_encode_load_wire_clamps);
  RUN_TEST(test_load_feedback_round_trip);
  RUN_TEST(test_load_feedback_rejects_wrong_type);
  RUN_TEST(test_gripper_baseline_subtracts_idle_offset);
  RUN_TEST(test_gripper_baseline_does_not_absorb_contact);
  return UNITY_END();
}
