#include <unity.h>

#include <cstring>

#include "../../src/common/calibration/calibration_profile_utils.h"
#include "../../src/common/teleop/teleop_espnow_batch_payload.h"
#include "../../src/common/teleop/teleop_espnow_legacy_batch_codec.h"
#include "../../src/common/teleop/teleop_espnow_turbo_compact_codec.h"
#include "../../src/common/teleop/teleop_espnow_turbo_compact_packet.h"
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

void test_turbo_codec_round_trip() {
  const TeleopEspNowBatchPayload source = makeSamplePayload();
  static const TeleopEspNowTurboCompactCodec codec;

  uint8_t buffer[32]{};
  size_t outLen = 0U;
  TEST_ASSERT_TRUE(codec.encode(source, buffer, sizeof(buffer), outLen));
  TEST_ASSERT_EQUAL_size_t(sizeof(teleop_espnow::TeleopEspNowTurboPacket), outLen);

  TeleopEspNowBatchPayload decoded{};
  TEST_ASSERT_TRUE(codec.decode(buffer, outLen, decoded));
  TEST_ASSERT_EQUAL_UINT8(source.count, decoded.count);
  TEST_ASSERT_EQUAL_UINT8(source.speedPct, decoded.speedPct);
  TEST_ASSERT_EQUAL_UINT16(source.requestId, decoded.requestId);
  TEST_ASSERT_TRUE(decoded.turbo);

  for (uint8_t i = 0U; i < source.count; ++i) {
    TEST_ASSERT_EQUAL_UINT8(source.ids[i], decoded.ids[i]);
    TEST_ASSERT_EQUAL_INT16(source.positions[i], decoded.positions[i]);
  }
}

void test_turbo_packet_size_is_compact() {
  TEST_ASSERT_EQUAL_UINT32(16U, static_cast<unsigned>(sizeof(teleop_espnow::TeleopEspNowTurboPacket)));
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
  TEST_ASSERT_GREATER_THAN(600, remapped);
  TEST_ASSERT_LESS_THAN(3200, remapped);

  TeleopEspNowBatchPayload payload{};
  payload.count = 1U;
  payload.speedPct = 100U;
  payload.requestId = 7U;
  payload.turbo = true;
  payload.ids[0] = 1U;
  payload.positions[0] = remapped;

  static const TeleopEspNowTurboCompactCodec codec;
  uint8_t buffer[32]{};
  size_t outLen = 0U;
  TEST_ASSERT_TRUE(codec.encode(payload, buffer, sizeof(buffer), outLen));

  TeleopEspNowBatchPayload decoded{};
  TEST_ASSERT_TRUE(codec.decode(buffer, outLen, decoded));
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
  RUN_TEST(test_turbo_codec_round_trip);
  RUN_TEST(test_turbo_packet_size_is_compact);
  RUN_TEST(test_legacy_codec_round_trip);
  RUN_TEST(test_remap_then_turbo_codec_preserves_follower_counts);
  RUN_TEST(test_turbo_decode_rejects_legacy_packet);
  return UNITY_END();
}
