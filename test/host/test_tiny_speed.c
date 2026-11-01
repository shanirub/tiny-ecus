#include <stdint.h>

#include "tiny_speed.h"
#include "unity.h"

// The firmware's values: 10 ms control period, 4320 counts per wheel turn.
#define PERIOD_MS  10
#define PER_TURN   4320

void setUp(void) {}
void tearDown(void) {}

static int32_t rpm_milli(int32_t counts)
{
    return tiny_speed_rpm_milli(counts, PERIOD_MS, PER_TURN);
}

static void test_no_counts_is_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, rpm_milli(0));
}

static void test_exact_speeds(void)
{
    // 72 counts per 10 ms = 7200 per s = 100000 per min = 100 turns per min.
    TEST_ASSERT_EQUAL_INT32(100000, rpm_milli(72));
    TEST_ASSERT_EQUAL_INT32(-100000, rpm_milli(-72));
    TEST_ASSERT_EQUAL_INT32(50000, rpm_milli(36));
}

static void test_rounds_to_nearest(void)
{
    // One count per period is 1388.89 milli-rpm, two are 2777.78: truncating
    // would give 1388 and 2777.
    TEST_ASSERT_EQUAL_INT32(1389, rpm_milli(1));
    TEST_ASSERT_EQUAL_INT32(-1389, rpm_milli(-1));
    TEST_ASSERT_EQUAL_INT32(2778, rpm_milli(2));
    TEST_ASSERT_EQUAL_INT32(-2778, rpm_milli(-2));
    // 5 counts are 6944.44: rounds down, not up.
    TEST_ASSERT_EQUAL_INT32(6944, rpm_milli(5));
    TEST_ASSERT_EQUAL_INT32(-6944, rpm_milli(-5));
}

static void test_halves_round_away_from_zero(void)
{
    // 1 count over 1000 ms with 24000 per turn is exactly 2.5 milli-rpm.
    TEST_ASSERT_EQUAL_INT32(3, tiny_speed_rpm_milli(1, 1000, 24000));
    TEST_ASSERT_EQUAL_INT32(-3, tiny_speed_rpm_milli(-1, 1000, 24000));
}

static void test_period_and_resolution_are_used(void)
{
    // The same counts over twice the time, or with twice the counts per turn,
    // is half the speed.
    TEST_ASSERT_EQUAL_INT32(50000, tiny_speed_rpm_milli(72, 20, PER_TURN));
    TEST_ASSERT_EQUAL_INT32(50000, tiny_speed_rpm_milli(72, PERIOD_MS, 2 * PER_TURN));
    // 48 counts per motor turn, 1 s: 1 turn per s = 60 rpm.
    TEST_ASSERT_EQUAL_INT32(60000, tiny_speed_rpm_milli(48, 1000, 48));
}

static void test_zero_divisor_is_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, tiny_speed_rpm_milli(100, 0, PER_TURN));
    TEST_ASSERT_EQUAL_INT32(0, tiny_speed_rpm_milli(100, PERIOD_MS, 0));
}

static void test_clamps_to_int32(void)
{
    // INT32_MAX counts in 1 ms at 1 per turn is far beyond the int32 range.
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, tiny_speed_rpm_milli(INT32_MAX, 1, 1));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, tiny_speed_rpm_milli(INT32_MIN, 1, 1));
    // Just inside: 35 counts in 1 ms at 1 per turn = 2.1e9 milli-rpm.
    TEST_ASSERT_EQUAL_INT32(2100000000, tiny_speed_rpm_milli(35, 1, 1));
    TEST_ASSERT_EQUAL_INT32(-2100000000, tiny_speed_rpm_milli(-35, 1, 1));
}

static void test_huge_divisor_is_zero(void)
{
    // period x counts_per_turn above INT64_MAX: any count is far below one
    // milli-rpm.
    TEST_ASSERT_EQUAL_INT32(0, tiny_speed_rpm_milli(INT32_MAX, UINT32_MAX, UINT32_MAX));
    TEST_ASSERT_EQUAL_INT32(0, tiny_speed_rpm_milli(INT32_MIN, UINT32_MAX, UINT32_MAX));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_counts_is_zero);
    RUN_TEST(test_exact_speeds);
    RUN_TEST(test_rounds_to_nearest);
    RUN_TEST(test_halves_round_away_from_zero);
    RUN_TEST(test_period_and_resolution_are_used);
    RUN_TEST(test_zero_divisor_is_zero);
    RUN_TEST(test_clamps_to_int32);
    RUN_TEST(test_huge_divisor_is_zero);
    return UNITY_END();
}
