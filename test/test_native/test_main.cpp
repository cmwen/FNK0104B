#include <unity.h>

void test_native_runner_smoke() {
  TEST_ASSERT_EQUAL_INT(42, 6 * 7);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_native_runner_smoke);
  return UNITY_END();
}
