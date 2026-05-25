#include <cstdlib>

#include <unity.h>

void test_python_teleop_non_regression_suite_passes() {
  const int rc = std::system("python tools/telemetry_dashboard/tests/test_teleop_runtime.py >nul 2>nul");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "Teleop non-regression python tests failed. Run tools/telemetry_dashboard/tests/test_teleop_runtime.py for details.");
}

void setUp() {
}

void tearDown() {
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();
  RUN_TEST(test_python_teleop_non_regression_suite_passes);
  return UNITY_END();
}
