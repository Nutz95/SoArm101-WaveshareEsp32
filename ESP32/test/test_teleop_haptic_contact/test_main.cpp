#include <unity.h>

#include "../../src/common/teleop/teleop_haptic_contact.h"

using namespace soarm::teleop_haptic;

void test_contact_requires_firm_load_and_gap() {
  TEST_ASSERT_TRUE(shouldEngageGripperHaptic(20U, 45));
  TEST_ASSERT_FALSE(shouldEngageGripperHaptic(20U, 30));
  TEST_ASSERT_FALSE(shouldEngageGripperHaptic(12U, 45));
  TEST_ASSERT_FALSE(shouldEngageGripperHaptic(4U, 45));
}

void test_contact_gap_is_absolute() {
  TEST_ASSERT_EQUAL_INT32(30, gripperLeaderFollowerAbsGap(100, 130));
  TEST_ASSERT_EQUAL_INT32(30, gripperLeaderFollowerAbsGap(130, 100));
}

void test_position_sync_only_on_firm_load() {
  TEST_ASSERT_FALSE(shouldSyncGripperPosition(12U));
  TEST_ASSERT_TRUE(shouldSyncGripperPosition(20U));
  TEST_ASSERT_EQUAL_INT16(500, selectGripperHapticGoal(500, 450, 12U));
  TEST_ASSERT_EQUAL_INT16(450, selectGripperHapticGoal(500, 450, 20U));
}

void test_mirror_catch_up_disengages() {
  TEST_ASSERT_TRUE(shouldDisengageMirrorCatchUp(8U, 10));
  TEST_ASSERT_FALSE(shouldDisengageMirrorCatchUp(20U, 10));
  TEST_ASSERT_FALSE(shouldDisengageMirrorCatchUp(8U, 30));
}

void setUp() {}

void tearDown() {}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_contact_requires_firm_load_and_gap);
  RUN_TEST(test_contact_gap_is_absolute);
  RUN_TEST(test_position_sync_only_on_firm_load);
  RUN_TEST(test_mirror_catch_up_disengages);
  return UNITY_END();
}
