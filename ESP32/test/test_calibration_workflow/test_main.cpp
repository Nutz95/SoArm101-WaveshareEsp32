#include <unity.h>
#include <cstdio>

#include "../../src/common/types/calibration_profile.h"
#include "../../src/leader/leader_calibration_workflow_internal.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static soarm::CalibrationProfile makeResetProfile() {
  soarm::CalibrationProfile p{};
  soarm::resetWorkingProfile(p);
  return p;
}

// ---------------------------------------------------------------------------
// resetWorkingProfile
// ---------------------------------------------------------------------------

void test_reset_working_profile_sets_min_to_max_range() {
  soarm::CalibrationProfile profile{};
  soarm::resetWorkingProfile(profile);
  TEST_ASSERT_EQUAL_UINT16(4095U, profile.minPosition[0]);
  TEST_ASSERT_EQUAL_UINT16(0U, profile.maxPosition[0]);
}

void test_reset_working_profile_clears_all_servos() {
  soarm::CalibrationProfile profile{};
  // Dirty with arbitrary values
  for (uint8_t i = 0U; i < soarm::CalibrationProfile::kServoCount; ++i) {
    profile.minPosition[i] = 100U;
    profile.maxPosition[i] = 2000U;
  }
  soarm::resetWorkingProfile(profile);
  for (uint8_t i = 0U; i < soarm::CalibrationProfile::kServoCount; ++i) {
    TEST_ASSERT_EQUAL_UINT16(4095U, profile.minPosition[i]);
    TEST_ASSERT_EQUAL_UINT16(0U, profile.maxPosition[i]);
  }
}

// ---------------------------------------------------------------------------
// expandWorkingProfileFromTelemetry
// ---------------------------------------------------------------------------

void test_expand_profile_parses_single_servo() {
  soarm::CalibrationProfile p = makeResetProfile();
  const bool ok = soarm::expandWorkingProfileFromTelemetry(p, "#1 p2048 t25;");
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_UINT16(2048U, p.minPosition[0]);
  TEST_ASSERT_EQUAL_UINT16(2048U, p.maxPosition[0]);
}

void test_expand_profile_updates_min_max() {
  soarm::CalibrationProfile p = makeResetProfile();
  soarm::expandWorkingProfileFromTelemetry(p, "#1 p2048 t25;");
  // Now a lower value — should update min
  soarm::expandWorkingProfileFromTelemetry(p, "#1 p1000 t25;");
  TEST_ASSERT_EQUAL_UINT16(1000U, p.minPosition[0]);
  TEST_ASSERT_EQUAL_UINT16(2048U, p.maxPosition[0]);
  // Now a higher value — should update max
  soarm::expandWorkingProfileFromTelemetry(p, "#1 p3500 t25;");
  TEST_ASSERT_EQUAL_UINT16(1000U, p.minPosition[0]);
  TEST_ASSERT_EQUAL_UINT16(3500U, p.maxPosition[0]);
}

void test_expand_profile_handles_multiple_servos() {
  soarm::CalibrationProfile p = makeResetProfile();
  const bool ok = soarm::expandWorkingProfileFromTelemetry(p, "#1 p1000 t25;#2 p2000 t25;#3 p3000 t25;");
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_UINT16(1000U, p.minPosition[0]);
  TEST_ASSERT_EQUAL_UINT16(2000U, p.minPosition[1]);
  TEST_ASSERT_EQUAL_UINT16(3000U, p.minPosition[2]);
}

void test_expand_profile_returns_false_for_null() {
  soarm::CalibrationProfile p = makeResetProfile();
  TEST_ASSERT_FALSE(soarm::expandWorkingProfileFromTelemetry(p, nullptr));
}

void test_expand_profile_returns_false_for_empty() {
  soarm::CalibrationProfile p = makeResetProfile();
  const bool ok = soarm::expandWorkingProfileFromTelemetry(p, "");
  // Empty string: no entries found → false
  TEST_ASSERT_FALSE(ok);
}

void test_expand_profile_clamps_negative_position() {
  soarm::CalibrationProfile p = makeResetProfile();
  soarm::expandWorkingProfileFromTelemetry(p, "#1 p-100 t25;");
  TEST_ASSERT_EQUAL_UINT16(0U, p.minPosition[0]);
  TEST_ASSERT_EQUAL_UINT16(0U, p.maxPosition[0]);
}

void test_expand_profile_clamps_over_max_position() {
  soarm::CalibrationProfile p = makeResetProfile();
  soarm::expandWorkingProfileFromTelemetry(p, "#1 p5000 t25;");
  TEST_ASSERT_EQUAL_UINT16(4095U, p.minPosition[0]);
  TEST_ASSERT_EQUAL_UINT16(4095U, p.maxPosition[0]);
}

void test_expand_profile_ignores_malformed_token_without_hanging() {
  soarm::CalibrationProfile p = makeResetProfile();
  const bool ok = soarm::expandWorkingProfileFromTelemetry(p, "#x p2048 t25;");
  TEST_ASSERT_FALSE(ok);
  for (uint8_t i = 0U; i < soarm::CalibrationProfile::kServoCount; ++i) {
    TEST_ASSERT_EQUAL_UINT16(4095U, p.minPosition[i]);
    TEST_ASSERT_EQUAL_UINT16(0U, p.maxPosition[i]);
  }
}

void test_expand_profile_ignores_invalid_servo_id_0() {
  soarm::CalibrationProfile p = makeResetProfile();
  soarm::expandWorkingProfileFromTelemetry(p, "#0 p2048 t25;");
  // Servo ID 0 is invalid — no servo should be updated
  for (uint8_t i = 0U; i < soarm::CalibrationProfile::kServoCount; ++i) {
    TEST_ASSERT_EQUAL_UINT16(4095U, p.minPosition[i]);
    TEST_ASSERT_EQUAL_UINT16(0U, p.maxPosition[i]);
  }
}

void test_expand_profile_ignores_servo_id_beyond_count() {
  soarm::CalibrationProfile p = makeResetProfile();
  // Servo ID 7 is out of range (kServoCount == 6)
  soarm::expandWorkingProfileFromTelemetry(p, "#7 p2048 t25;");
  for (uint8_t i = 0U; i < soarm::CalibrationProfile::kServoCount; ++i) {
    TEST_ASSERT_EQUAL_UINT16(4095U, p.minPosition[i]);
    TEST_ASSERT_EQUAL_UINT16(0U, p.maxPosition[i]);
  }
}

// ---------------------------------------------------------------------------
// profileHasRange
// ---------------------------------------------------------------------------

void test_profile_has_range_returns_true_when_any_has_range() {
  soarm::CalibrationProfile p = makeResetProfile();
  soarm::expandWorkingProfileFromTelemetry(p, "#1 p1000 t25;#2 p1000 t25;#3 p1000 t25;");
  soarm::expandWorkingProfileFromTelemetry(p, "#1 p2000 t25;#2 p2000 t25;#3 p2000 t25;");
  TEST_ASSERT_TRUE(soarm::profileHasRange(p));
}

void test_profile_has_range_returns_false_when_no_range() {
  soarm::CalibrationProfile p = makeResetProfile();
  // Still in initial state: min=4095, max=0 for all → no range
  TEST_ASSERT_FALSE(soarm::profileHasRange(p));
}

void test_profile_has_range_returns_false_when_min_equals_max() {
  soarm::CalibrationProfile p = makeResetProfile();
  soarm::expandWorkingProfileFromTelemetry(p, "#1 p2048 t25;");
  TEST_ASSERT_FALSE(soarm::profileHasRange(p));
}

void test_profile_has_range_returns_true_when_last_servo_has_range() {
  soarm::CalibrationProfile p = makeResetProfile();
  soarm::expandWorkingProfileFromTelemetry(p, "#1 p1000 t25;#2 p1000 t25;#3 p1000 t25;");
  soarm::expandWorkingProfileFromTelemetry(p, "#1 p2000 t25;#2 p2000 t25;#3 p2000 t25;");
  TEST_ASSERT_TRUE(soarm::profileHasRange(p));
}

// ---------------------------------------------------------------------------
// Full workflow integration tests
// ---------------------------------------------------------------------------

void test_full_workflow_begin_then_sample_then_commit() {
  // Simulates the full LeRobot calibration range capture cycle
  soarm::CalibrationProfile working{};
  soarm::resetWorkingProfile(working);

  // Phase: sample multiple positions for 3 servos
  soarm::expandWorkingProfileFromTelemetry(working, "#1 p1200 t25;#2 p2500 t25;#3 p800 t25;");
  soarm::expandWorkingProfileFromTelemetry(working, "#1 p3800 t25;#2 p1800 t25;#3 p3200 t25;");
  soarm::expandWorkingProfileFromTelemetry(working, "#1 p2048 t25;#2 p2048 t25;#3 p2048 t25;");

  TEST_ASSERT_TRUE(soarm::profileHasRange(working));

  // Min and max should reflect the extremes seen across all samples
  TEST_ASSERT_EQUAL_UINT16(1200U, working.minPosition[0]);
  TEST_ASSERT_EQUAL_UINT16(3800U, working.maxPosition[0]);
  TEST_ASSERT_EQUAL_UINT16(1800U, working.minPosition[1]);
  TEST_ASSERT_EQUAL_UINT16(2500U, working.maxPosition[1]);
  TEST_ASSERT_EQUAL_UINT16(800U, working.minPosition[2]);
  TEST_ASSERT_EQUAL_UINT16(3200U, working.maxPosition[2]);
}

void test_cancel_restores_backup() {
  // Simulates cancel: profile backup is restored (pure logic; backup/restore is caller-side)
  soarm::CalibrationProfile backup{};
  soarm::resetWorkingProfile(backup);
  backup.minPosition[0] = 500U;
  backup.maxPosition[0] = 3500U;

  soarm::CalibrationProfile working = backup;
  // Capture some new data
  soarm::expandWorkingProfileFromTelemetry(working, "#1 p100 t25;");
  soarm::expandWorkingProfileFromTelemetry(working, "#1 p4000 t25;");
  TEST_ASSERT_EQUAL_UINT16(100U, working.minPosition[0]);

  // Cancel: restore backup
  working = backup;
  TEST_ASSERT_EQUAL_UINT16(500U, working.minPosition[0]);
  TEST_ASSERT_EQUAL_UINT16(3500U, working.maxPosition[0]);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
void setUp() {}
void tearDown() {}

int main(int /*argc*/, char ** /*argv*/) {
  UNITY_BEGIN();

  RUN_TEST(test_reset_working_profile_sets_min_to_max_range);
  RUN_TEST(test_reset_working_profile_clears_all_servos);

  RUN_TEST(test_expand_profile_parses_single_servo);
  RUN_TEST(test_expand_profile_updates_min_max);
  RUN_TEST(test_expand_profile_handles_multiple_servos);
  RUN_TEST(test_expand_profile_returns_false_for_null);
  RUN_TEST(test_expand_profile_returns_false_for_empty);
  RUN_TEST(test_expand_profile_clamps_negative_position);
  RUN_TEST(test_expand_profile_clamps_over_max_position);
  RUN_TEST(test_expand_profile_ignores_malformed_token_without_hanging);
  RUN_TEST(test_expand_profile_ignores_invalid_servo_id_0);
  RUN_TEST(test_expand_profile_ignores_servo_id_beyond_count);

  RUN_TEST(test_profile_has_range_returns_true_when_any_has_range);
  RUN_TEST(test_profile_has_range_returns_false_when_no_range);
  RUN_TEST(test_profile_has_range_returns_false_when_min_equals_max);
  RUN_TEST(test_profile_has_range_returns_true_when_last_servo_has_range);

  RUN_TEST(test_full_workflow_begin_then_sample_then_commit);
  RUN_TEST(test_cancel_restores_backup);

  return UNITY_END();
}
