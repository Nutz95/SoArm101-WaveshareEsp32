#include <unity.h>

#include "../../src/common/app_state_machine.h"

static soarm::ArmStateMachine g_stateMachine;

void setUp() {
}

void tearDown() {
}

void test_leader_is_pairing_when_joystick_not_paired() {
  const soarm::ArmStateInputs inputs{true, false, false, false};
  const soarm::ArmRuntimeState state = g_stateMachine.computeState(inputs);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(soarm::ArmRuntimeState::PairingOrUnpaired), static_cast<int>(state));
}

void test_leader_is_waiting_calibration_when_paired_without_calibration() {
  const soarm::ArmStateInputs inputs{true, true, false, false};
  const soarm::ArmRuntimeState state = g_stateMachine.computeState(inputs);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(soarm::ArmRuntimeState::WaitingCalibration), static_cast<int>(state));
}

void test_arm_is_ready_when_calibrated_and_linked() {
  const soarm::ArmStateInputs inputs{false, false, true, true};
  const soarm::ArmRuntimeState state = g_stateMachine.computeState(inputs);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(soarm::ArmRuntimeState::Ready), static_cast<int>(state));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_leader_is_pairing_when_joystick_not_paired);
  RUN_TEST(test_leader_is_waiting_calibration_when_paired_without_calibration);
  RUN_TEST(test_arm_is_ready_when_calibrated_and_linked);
  return UNITY_END();
}
