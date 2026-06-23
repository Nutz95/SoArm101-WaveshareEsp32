#include <unity.h>

#include <cstring>

#include "../../src/common/calibration/calibration_profile_utils.h"
#include "../../src/common/teleop/teleop_espnow_batch_payload.h"
#include "../../src/common/teleop/teleop_espnow_legacy_batch_codec.h"
#include "../../src/common/teleop/teleop_espnow_turbo_compact_codec.h"
#include "../../src/common/teleop/teleop_espnow_turbo_compact_packet.h"
#include "../../src/common/teleop/teleop_espnow_turbo_config.h"
#include "../../src/common/teleop/teleop_espnow_turbo_keyframe_policy.h"
#include "../../src/common/teleop/teleop_espnow_turbo_session.h"
#include "../../src/common/teleop/teleop_position_12bit_pack.h"

using namespace soarm;

namespace {

TeleopEspNowBatchPayload makeSamplePayload() {
  TeleopEspNowBatchPayload payload{};
  payload.count = 3U;
  payload.speedPct = 80U;
  payload.requestId = 0x1234U;
  payload.turbo = true;
  payload.ids[0] = 1U;
  payload.ids[1] = 3U;
  payload.ids[2] = 6U;
  payload.positions[0] = 100U;
  payload.positions[1] = 2048U;
  payload.positions[2] = 4095U;
  return payload;
}

TeleopEspNowBatchPayload makeSingleAxisPayload(uint8_t id, int16_t position) {
  TeleopEspNowBatchPayload payload{};
  payload.count = 1U;
  payload.speedPct = 100U;
  payload.requestId = 42U;
  payload.turbo = true;
  payload.ids[0] = id;
  payload.positions[0] = position;
  return payload;
}

} // namespace

void test_pack6_slots12_round_trip() {
  uint16_t source[teleop_position_pack::kSlotCount] = {0U, 512U, 1024U, 2048U, 3000U, 4095U};
  uint8_t packed[teleop_position_pack::kPackedByteLength]{};
  uint16_t restored[teleop_position_pack::kSlotCount]{};

  teleop_position_pack::pack6Slots12Bit(source, packed);
  teleop_position_pack::unpack6Slots12Bit(packed, restored);

  for (uint8_t i = 0U; i < teleop_position_pack::kSlotCount; ++i) {
    TEST_ASSERT_EQUAL_UINT16(source[i], restored[i]);
  }
}

void test_pack_masked_slots_round_trip() {
  const uint8_t mask = 0b0010101U;
  uint16_t slots[teleop_position_pack::kSlotCount] = {10U, 0U, 20U, 0U, 30U, 0U};
  uint8_t packed[teleop_position_pack::kPackedByteLength]{};
  uint16_t restored[teleop_position_pack::kSlotCount]{};

  teleop_position_pack::packMaskedSlots12Bit(mask, slots, packed, sizeof(packed));
  TEST_ASSERT_TRUE(
      teleop_position_pack::unpackMaskedSlots12Bit(mask, packed, teleop_position_pack::packedByteLengthForMask(mask), restored));

  TEST_ASSERT_EQUAL_UINT16(10U, restored[0]);
  TEST_ASSERT_EQUAL_UINT16(20U, restored[2]);
  TEST_ASSERT_EQUAL_UINT16(30U, restored[4]);
}

void test_speed_pct_uses_seven_bits() {
  TEST_ASSERT_EQUAL_UINT8(100U, teleop_espnow::decodeSpeedPct(teleop_espnow::encodeControlByte(100U, false)));
  TEST_ASSERT_EQUAL_UINT8(100U, teleop_espnow::decodeSpeedPct(teleop_espnow::encodeControlByte(127U, false)));
  TEST_ASSERT_TRUE(teleop_espnow::decodeKeyframeFlag(teleop_espnow::encodeControlByte(50U, true)));
}

void test_turbo_v2_session_round_trip() {
  static const TeleopEspNowTurboCompactCodec codec;
  TeleopEspNowTurboSession leaderSession{};
  TeleopEspNowTurboSession followerSession{};
  const TeleopEspNowBatchPayload source = makeSamplePayload();
  const uint8_t expectedMask = 0x25U;

  uint8_t buffer[32]{};
  size_t outLen = 0U;
  TEST_ASSERT_TRUE(codec.encodeWithSession(source, leaderSession, 1000U, buffer, sizeof(buffer), outLen));
  TEST_ASSERT_EQUAL_size_t(
      teleop_espnow::kTurboHeaderV2Size + teleop_position_pack::packedByteLengthForMask(expectedMask),
      outLen);
  TEST_ASSERT_TRUE(teleop_espnow::decodeKeyframeFlag(buffer[6]));

  TeleopEspNowBatchPayload decoded{};
  TEST_ASSERT_TRUE(codec.decodeWithSession(buffer, outLen, followerSession, decoded));
  TEST_ASSERT_EQUAL_UINT8(source.count, decoded.count);
  TEST_ASSERT_EQUAL_UINT8(source.speedPct, decoded.speedPct);
  TEST_ASSERT_EQUAL_UINT16(source.requestId, decoded.requestId);
  TEST_ASSERT_TRUE(decoded.turbo);

  for (uint8_t i = 0U; i < source.count; ++i) {
    TEST_ASSERT_EQUAL_UINT8(source.ids[i], decoded.ids[i]);
    TEST_ASSERT_EQUAL_INT16(source.positions[i], decoded.positions[i]);
  }
}

void test_session_merge_three_axis_mask() {
  TeleopEspNowTurboSession session{};
  session.mergePayload(makeSamplePayload());
  TEST_ASSERT_EQUAL_UINT8(0x25U, session.knownMask);
}

void test_turbo_sparse_delta_smaller_than_keyframe() {
  static const TeleopEspNowTurboCompactCodec codec;
  TeleopEspNowTurboSession leaderSession{};

  uint8_t keyframeBuffer[32]{};
  size_t keyframeLen = 0U;
  TEST_ASSERT_TRUE(codec.encodeWithSession(
      makeSamplePayload(), leaderSession, 1000U, keyframeBuffer, sizeof(keyframeBuffer), keyframeLen));
  TEST_ASSERT_TRUE(teleop_espnow::decodeKeyframeFlag(keyframeBuffer[6]));
  TEST_ASSERT_EQUAL_size_t(12U, keyframeLen);
  TEST_ASSERT_EQUAL_UINT8(0x25U, keyframeBuffer[3]);
  TEST_ASSERT_TRUE(leaderSession.hasBaselineKeyframe);
  TEST_ASSERT_EQUAL_UINT32(1000U, leaderSession.lastKeyframeMs);
  TEST_ASSERT_EQUAL_UINT8(0U, leaderSession.framesSinceKeyframe);

  uint8_t deltaBuffer[32]{};
  size_t deltaLen = 0U;
  TEST_ASSERT_TRUE(codec.encodeWithSession(
      makeSingleAxisPayload(2U, 1500),
      leaderSession,
      1010U,
      deltaBuffer,
      sizeof(deltaBuffer),
      deltaLen));
  TEST_ASSERT_FALSE(teleop_espnow::decodeKeyframeFlag(deltaBuffer[6]));
  TEST_ASSERT_EQUAL_size_t(9U, deltaLen);
  TEST_ASSERT_EQUAL_UINT8(0x02U, deltaBuffer[3]);
  TEST_ASSERT_LESS_THAN(keyframeLen, deltaLen);
  TeleopEspNowTurboSession followerSession{};
  TeleopEspNowBatchPayload decoded{};
  TEST_ASSERT_TRUE(codec.decodeWithSession(deltaBuffer, deltaLen, followerSession, decoded));
  TEST_ASSERT_EQUAL_UINT8(1U, decoded.count);
  TEST_ASSERT_EQUAL_UINT8(2U, decoded.ids[0]);
  TEST_ASSERT_EQUAL_INT16(1500, decoded.positions[0]);
}

void test_keyframe_policy_forces_resync() {
  TeleopEspNowTurboSession session{};
  session.hasBaselineKeyframe = true;
  session.knownMask = 0x3FU;
  session.framesSinceKeyframe = teleop_espnow::kTurboKeyframeEveryNFrames;
  session.lastKeyframeMs = 1000U;
  TEST_ASSERT_TRUE(TeleopEspNowTurboKeyframePolicy::shouldSendKeyframe(session, 1100U));
}

void test_keyframe_policy_requires_baseline_first() {
  TeleopEspNowTurboSession session{};
  TEST_ASSERT_TRUE(TeleopEspNowTurboKeyframePolicy::shouldSendKeyframe(session, 0U));
}

void test_turbo_rejects_wrong_wire_version() {
  static const TeleopEspNowTurboCompactCodec codec;
  TeleopEspNowTurboSession session{};

  uint8_t buffer[32]{};
  size_t outLen = 0U;
  TEST_ASSERT_TRUE(codec.encodeWithSession(makeSamplePayload(), session, 1000U, buffer, sizeof(buffer), outLen));
  buffer[1] = 99U;

  TeleopEspNowBatchPayload decoded{};
  TEST_ASSERT_FALSE(codec.decodeWithSession(buffer, outLen, session, decoded));
}

void test_legacy_codec_round_trip() {
  TeleopEspNowBatchPayload source = makeSamplePayload();
  source.turbo = false;
  static const TeleopEspNowLegacyBatchCodec codec;

  uint8_t buffer[256]{};
  size_t outLen = 0U;
  TEST_ASSERT_TRUE(codec.encode(source, buffer, sizeof(buffer), outLen));
  TEST_ASSERT_EQUAL_size_t(172U, outLen);

  TeleopEspNowBatchPayload decoded{};
  TEST_ASSERT_TRUE(codec.decode(buffer, outLen, decoded));
  TEST_ASSERT_EQUAL_UINT8(source.count, decoded.count);
  TEST_ASSERT_FALSE(decoded.turbo);
  TEST_ASSERT_EQUAL_INT16(source.positions[1], decoded.positions[1]);
}

void test_remap_then_turbo_codec_preserves_follower_counts() {
  CalibrationProfile leader{};
  CalibrationProfile follower{};
  for (uint8_t i = 0U; i < CalibrationProfile::kServoCount; ++i) {
    leader.minPosition[i] = 500U;
    leader.maxPosition[i] = 3500U;
    follower.minPosition[i] = 600U;
    follower.maxPosition[i] = 3200U;
  }

  const int16_t remapped = remapServoPositionWithCalibration(1U, 2000, leader, follower);
  TeleopEspNowBatchPayload payload = makeSingleAxisPayload(1U, remapped);

  static const TeleopEspNowTurboCompactCodec codec;
  TeleopEspNowTurboSession leaderSession{};
  TeleopEspNowTurboSession followerSession{};
  uint8_t buffer[32]{};
  size_t outLen = 0U;
  TEST_ASSERT_TRUE(codec.encodeWithSession(payload, leaderSession, 500U, buffer, sizeof(buffer), outLen));

  TeleopEspNowBatchPayload decoded{};
  TEST_ASSERT_TRUE(codec.decodeWithSession(buffer, outLen, followerSession, decoded));
  TEST_ASSERT_EQUAL_INT16(remapped, decoded.positions[0]);
}

void test_turbo_decode_rejects_legacy_packet() {
  static const TeleopEspNowLegacyBatchCodec legacyCodec;
  static const TeleopEspNowTurboCompactCodec turboCodec;

  TeleopEspNowBatchPayload source = makeSamplePayload();
  source.turbo = false;
  uint8_t buffer[256]{};
  size_t outLen = 0U;
  TEST_ASSERT_TRUE(legacyCodec.encode(source, buffer, sizeof(buffer), outLen));

  TeleopEspNowBatchPayload decoded{};
  TEST_ASSERT_FALSE(turboCodec.decode(buffer, outLen, decoded));
}

void setUp() {
}

void tearDown() {
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_pack6_slots12_round_trip);
  RUN_TEST(test_pack_masked_slots_round_trip);
  RUN_TEST(test_speed_pct_uses_seven_bits);
  RUN_TEST(test_turbo_v2_session_round_trip);
  RUN_TEST(test_session_merge_three_axis_mask);
  RUN_TEST(test_turbo_sparse_delta_smaller_than_keyframe);
  RUN_TEST(test_keyframe_policy_forces_resync);
  RUN_TEST(test_keyframe_policy_requires_baseline_first);
  RUN_TEST(test_turbo_rejects_wrong_wire_version);
  RUN_TEST(test_legacy_codec_round_trip);
  RUN_TEST(test_remap_then_turbo_codec_preserves_follower_counts);
  RUN_TEST(test_turbo_decode_rejects_legacy_packet);
  return UNITY_END();
}
