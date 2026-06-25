#include <unity.h>

#include "../../src/common/presence/presence_message_type.h"
#include "../../src/common/teleop/teleop_load_feedback_codec.h"

using namespace soarm;
using namespace soarm::teleop_load_feedback;

void test_encode_load_wire_clamps() {
  TEST_ASSERT_EQUAL_INT8(127, encodeLoadWire(2000));
  TEST_ASSERT_EQUAL_INT8(-128, encodeLoadWire(-2000));
  TEST_ASSERT_EQUAL_INT8(50, encodeLoadWire(500));
}

void test_load_feedback_round_trip() {
  int8_t loads[6] = {1, -2, 3, 10, 20, 127};
  uint8_t buffer[kLoadFeedbackWireSize]{};
  size_t outLen = 0U;

  TEST_ASSERT_TRUE(encodePacket(42U, 7U, loads, buffer, sizeof(buffer), outLen));
  TEST_ASSERT_EQUAL_size_t(kLoadFeedbackWireSize, outLen);
  TEST_ASSERT_TRUE(isTeleopLoadFeedbackPacket(buffer, outLen));

  uint16_t requestId = 0U;
  uint8_t seq = 0U;
  int8_t decoded[6]{};
  TEST_ASSERT_TRUE(decodePacket(buffer, outLen, requestId, seq, decoded));
  TEST_ASSERT_EQUAL_UINT16(42U, requestId);
  TEST_ASSERT_EQUAL_UINT8(7U, seq);
  TEST_ASSERT_EQUAL_INT8_ARRAY(loads, decoded, 6);
}

void test_load_feedback_rejects_wrong_type() {
  uint8_t buffer[kLoadFeedbackWireSize]{};
  size_t outLen = 0U;
  int8_t loads[6]{};
  TEST_ASSERT_TRUE(encodePacket(1U, 0U, loads, buffer, sizeof(buffer), outLen));
  buffer[2] = static_cast<uint8_t>(PresenceMessageType::TeleopMirrorCompact);
  TEST_ASSERT_FALSE(isTeleopLoadFeedbackPacket(buffer, outLen));
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
  return UNITY_END();
}
