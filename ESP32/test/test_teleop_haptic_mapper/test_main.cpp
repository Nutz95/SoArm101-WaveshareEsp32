#include <unity.h>

#include "../../src/common/teleop/teleop_haptic_mapper.h"

using namespace soarm::teleop_haptic;

void test_haptic_deadband_uses_min_torque() {
  TEST_ASSERT_EQUAL_UINT16(kTorqueLimitMin, mapWireLoadToTorqueLimit(0U, false));
  TEST_ASSERT_EQUAL_UINT16(kTorqueLimitMin, mapWireLoadToTorqueLimit(kWireLoadDeadband, false));
}

void test_haptic_maps_high_load() {
  TEST_ASSERT_EQUAL_UINT16(kTorqueLimitMax, mapWireLoadToTorqueLimit(127U, false));
  TEST_ASSERT_EQUAL_UINT16(kGripperTorqueLimitMax, mapWireLoadToTorqueLimit(127U, true));
}

void test_haptic_gripper_gain_boosts() {
  const uint16_t base = mapWireLoadToTorqueLimit(64U, false);
  const uint16_t grip = mapWireLoadToTorqueLimit(64U, true);
  TEST_ASSERT_GREATER_THAN_UINT16(base, grip);
}

void setUp() {}

void tearDown() {}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_haptic_deadband_uses_min_torque);
  RUN_TEST(test_haptic_maps_high_load);
  RUN_TEST(test_haptic_gripper_gain_boosts);
  return UNITY_END();
}
