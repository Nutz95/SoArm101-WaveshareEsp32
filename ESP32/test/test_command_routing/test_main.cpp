#include <unity.h>

#include "leader/leader_command_processor.h"

using soarm::LeaderCommandAction;
using soarm::LeaderCommandProcessor;

static LeaderCommandProcessor::CommandFrame make_frame(uint8_t command_id) {
  LeaderCommandProcessor::CommandFrame frame{};
  frame.magic = LeaderCommandProcessor::kMagic;
  frame.version = LeaderCommandProcessor::kVersion;
  frame.command = command_id;
  frame.requestId = 42U;
  frame.value = 123456U;
  return frame;
}

void test_maps_servo_scan_leader_command() {
  LeaderCommandProcessor processor;
  const auto action = processor.process(make_frame(13U));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LeaderCommandAction::ServoScanLeader), static_cast<int>(action));
}

void test_maps_servo_scan_follower_command() {
  LeaderCommandProcessor processor;
  const auto action = processor.process(make_frame(14U));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LeaderCommandAction::ServoScanFollower), static_cast<int>(action));
}

void test_maps_follower_debug_enable_command() {
  LeaderCommandProcessor processor;
  const auto action = processor.process(make_frame(11U));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LeaderCommandAction::ServoDebugEnableFollower), static_cast<int>(action));
}

void test_maps_teleop_mirror_command() {
  LeaderCommandProcessor processor;
  const auto action = processor.process(make_frame(15U));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LeaderCommandAction::TeleopMirror), static_cast<int>(action));
}

void test_maps_teleop_continuous_set_command() {
  LeaderCommandProcessor processor;
  const auto action = processor.process(make_frame(16U));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LeaderCommandAction::TeleopContinuousSet), static_cast<int>(action));
}

void test_rejects_unknown_command() {
  LeaderCommandProcessor processor;
  const auto action = processor.process(make_frame(200U));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LeaderCommandAction::None), static_cast<int>(action));
}

void setUp() {
}

void tearDown() {
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_maps_servo_scan_leader_command);
  RUN_TEST(test_maps_servo_scan_follower_command);
  RUN_TEST(test_maps_follower_debug_enable_command);
  RUN_TEST(test_maps_teleop_mirror_command);
  RUN_TEST(test_maps_teleop_continuous_set_command);
  RUN_TEST(test_rejects_unknown_command);
  return UNITY_END();
}
