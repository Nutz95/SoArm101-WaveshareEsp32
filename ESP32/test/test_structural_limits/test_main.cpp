#include <cstdlib>

#include <unity.h>

void test_structural_limits_script_passes() {
  const int rc = std::system("python tools/check_structural_limits.py --project-root . >nul 2>nul");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "Structural limits script failed. Run tools/check_structural_limits.py for details.");
}

void setUp() {
}

void tearDown() {
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();
  RUN_TEST(test_structural_limits_script_passes);
  return UNITY_END();
}
