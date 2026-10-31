#include <stdint.h>

#include "tiny_motor.h"
#include "unity.h"

// Same shape as the firmware's settings, small enough to count by hand.
static const tiny_motor_cfg_t CFG = {
    .cap = 800,
    .slew = 50,
    .stall_duty = 300,
    .stall_counts = 1,
    .stall_steps = 30,
};
#define MOVING  10  // counts per step of a wheel that turns

static tiny_motor_t s_m;

void setUp(void)
{
    tiny_motor_init(&s_m);
}

void tearDown(void) {}

// Step `n` times with the wheel turning; returns the last duty.
static int32_t steps(int n, int32_t counts)
{
    int32_t duty = 0;
    for (int i = 0; i < n; i++) {
        duty = tiny_motor_step(&s_m, &CFG, counts);
    }
    return duty;
}

// --- Idle and runs ---

static void test_init_is_idle_and_off(void)
{
    TEST_ASSERT_EQUAL(TINY_MOTOR_IDLE, s_m.mode);
    TEST_ASSERT_EQUAL_INT32(0, tiny_motor_step(&s_m, &CFG, 0));
    TEST_ASSERT_EQUAL(TINY_MOTOR_IDLE, s_m.mode);
}

static void test_ramps_up_by_slew_then_holds(void)
{
    TEST_ASSERT_TRUE(tiny_motor_run(&s_m, &CFG, 200, 100));
    TEST_ASSERT_EQUAL_INT32(50, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL_INT32(100, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL_INT32(150, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL_INT32(200, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL_INT32(200, steps(10, MOVING));
}

static void test_reverse_ramps_down_by_slew(void)
{
    tiny_motor_run(&s_m, &CFG, -120, 100);
    TEST_ASSERT_EQUAL_INT32(-50, tiny_motor_step(&s_m, &CFG, -MOVING));
    TEST_ASSERT_EQUAL_INT32(-100, tiny_motor_step(&s_m, &CFG, -MOVING));
    TEST_ASSERT_EQUAL_INT32(-120, tiny_motor_step(&s_m, &CFG, -MOVING));  // last step smaller
}

static void test_duty_is_capped_both_ways(void)
{
    tiny_motor_run(&s_m, &CFG, 1000, 1000);
    TEST_ASSERT_EQUAL_INT32(800, steps(40, MOVING));
    tiny_motor_run(&s_m, &CFG, -5000, 1000);
    TEST_ASSERT_EQUAL_INT32(-800, steps(40, -MOVING));
}

// A run of N steps holds its duty for N-1 steps; the N-th starts the ramp down,
// and the mode turns IDLE in the step that reaches 0.
static void test_run_ends_with_ramp_then_idle(void)
{
    tiny_motor_run(&s_m, &CFG, 100, 10);
    TEST_ASSERT_EQUAL_INT32(100, steps(9, MOVING));
    TEST_ASSERT_EQUAL_INT32(50, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);
    TEST_ASSERT_EQUAL_INT32(0, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL(TINY_MOTOR_IDLE, s_m.mode);
}

static void test_reversing_ramps_through_zero(void)
{
    tiny_motor_run(&s_m, &CFG, 100, 100);
    steps(2, MOVING);
    tiny_motor_run(&s_m, &CFG, -100, 100);
    TEST_ASSERT_EQUAL_INT32(50, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL_INT32(0, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);  // passing 0 isn't the end of the run
    TEST_ASSERT_EQUAL_INT32(-50, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL_INT32(-100, tiny_motor_step(&s_m, &CFG, -MOVING));
}

static void test_zero_step_run_ramps_down(void)
{
    tiny_motor_run(&s_m, &CFG, 100, 100);
    steps(2, MOVING);
    tiny_motor_run(&s_m, &CFG, 500, 0);
    TEST_ASSERT_EQUAL_INT32(50, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL_INT32(0, tiny_motor_step(&s_m, &CFG, MOVING));
    TEST_ASSERT_EQUAL(TINY_MOTOR_IDLE, s_m.mode);
}

static void test_stop_is_immediate(void)
{
    tiny_motor_run(&s_m, &CFG, 400, 100);
    steps(8, MOVING);
    tiny_motor_stop(&s_m);
    TEST_ASSERT_EQUAL(TINY_MOTOR_IDLE, s_m.mode);
    TEST_ASSERT_EQUAL_INT32(0, tiny_motor_step(&s_m, &CFG, MOVING));
    tiny_motor_run(&s_m, &CFG, 400, 100);
    TEST_ASSERT_EQUAL_INT32(50, tiny_motor_step(&s_m, &CFG, MOVING));  // a new run ramps from 0
}

// --- Stall detection ---

static void test_stall_trips_after_stall_steps(void)
{
    tiny_motor_run(&s_m, &CFG, 500, 1000);
    steps(10, MOVING);  // at 500
    TEST_ASSERT_EQUAL_INT32(500, steps(29, 0));
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);
    TEST_ASSERT_EQUAL_INT32(0, tiny_motor_step(&s_m, &CFG, 0));
    TEST_ASSERT_EQUAL(TINY_MOTOR_FAULT, s_m.mode);
    TEST_ASSERT_EQUAL_INT32(0, steps(5, 0));  // stays off
}

static void test_movement_resets_the_stall_count(void)
{
    tiny_motor_run(&s_m, &CFG, 500, 1000);
    steps(10, MOVING);
    steps(29, 0);
    steps(1, MOVING);
    steps(29, 0);
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);
}

static void test_no_stall_below_stall_duty(void)
{
    tiny_motor_run(&s_m, &CFG, 250, 1000);
    steps(100, 0);
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);
}

// The counts belong to the period that ran at the previous duty. Ramping to
// 300 takes 6 steps; only the steps after that count, so the 36th step trips.
static void test_stall_judges_the_previous_period(void)
{
    tiny_motor_run(&s_m, &CFG, 300, 1000);
    steps(35, 0);
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);
    tiny_motor_step(&s_m, &CFG, 0);
    TEST_ASSERT_EQUAL(TINY_MOTOR_FAULT, s_m.mode);
}

static void test_stall_counts_threshold_both_directions(void)
{
    tiny_motor_run(&s_m, &CFG, 500, 1000);
    steps(10, MOVING);
    steps(30, 2);  // 2 counts per step is moving
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);
    steps(30, -2);
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);
    steps(15, 1);  // 1 count either way is not
    steps(15, -1);
    TEST_ASSERT_EQUAL(TINY_MOTOR_FAULT, s_m.mode);
}

// Resending DUTY to a stuck wheel must not hold off the trip.
static void test_new_run_keeps_the_stall_count(void)
{
    tiny_motor_run(&s_m, &CFG, 500, 1000);
    steps(10, MOVING);
    steps(20, 0);
    tiny_motor_run(&s_m, &CFG, 500, 1000);
    steps(9, 0);
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);
    tiny_motor_step(&s_m, &CFG, 0);
    TEST_ASSERT_EQUAL(TINY_MOTOR_FAULT, s_m.mode);
}

// The extremes are moving, and taking their absolute value would overflow
// (UBSan fails the test).
static void test_extreme_counts_are_moving(void)
{
    tiny_motor_run(&s_m, &CFG, 500, 1000);
    steps(10, MOVING);
    steps(40, INT32_MIN);
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);
    steps(40, INT32_MAX);
    TEST_ASSERT_EQUAL(TINY_MOTOR_RUN, s_m.mode);
}

static void test_fault_refuses_runs_until_stop(void)
{
    tiny_motor_run(&s_m, &CFG, 500, 1000);
    steps(10, MOVING);
    steps(30, 0);
    TEST_ASSERT_FALSE(tiny_motor_run(&s_m, &CFG, 200, 100));
    TEST_ASSERT_EQUAL(TINY_MOTOR_FAULT, s_m.mode);
    TEST_ASSERT_EQUAL_INT32(0, tiny_motor_step(&s_m, &CFG, 0));
    tiny_motor_stop(&s_m);
    TEST_ASSERT_TRUE(tiny_motor_run(&s_m, &CFG, 200, 100));
    TEST_ASSERT_EQUAL_INT32(50, tiny_motor_step(&s_m, &CFG, MOVING));  // ramps from 0 again
}

// --- Pin mapping (slow decay) ---

static void check_pins(int32_t duty, uint32_t in1, uint32_t in2)
{
    uint32_t a, b;
    tiny_motor_pins(duty, 1024, &a, &b);
    TEST_ASSERT_EQUAL_UINT32(in1, a);
    TEST_ASSERT_EQUAL_UINT32(in2, b);
}

static void test_pins_slow_decay(void)
{
    check_pins(0, 1024, 1024);      // both high: brake
    check_pins(1000, 1024, 0);      // forward: IN1 high, IN2 low
    check_pins(-1000, 0, 1024);     // reverse: IN1 low, IN2 high
    check_pins(250, 1024, 768);     // IN2 low a quarter of the time
    check_pins(-500, 512, 1024);
}

static void test_pins_clamp_out_of_range(void)
{
    check_pins(2000, 1024, 0);
    check_pins(-2000, 0, 1024);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_is_idle_and_off);
    RUN_TEST(test_ramps_up_by_slew_then_holds);
    RUN_TEST(test_reverse_ramps_down_by_slew);
    RUN_TEST(test_duty_is_capped_both_ways);
    RUN_TEST(test_run_ends_with_ramp_then_idle);
    RUN_TEST(test_reversing_ramps_through_zero);
    RUN_TEST(test_zero_step_run_ramps_down);
    RUN_TEST(test_stop_is_immediate);
    RUN_TEST(test_stall_trips_after_stall_steps);
    RUN_TEST(test_movement_resets_the_stall_count);
    RUN_TEST(test_no_stall_below_stall_duty);
    RUN_TEST(test_stall_judges_the_previous_period);
    RUN_TEST(test_stall_counts_threshold_both_directions);
    RUN_TEST(test_new_run_keeps_the_stall_count);
    RUN_TEST(test_extreme_counts_are_moving);
    RUN_TEST(test_fault_refuses_runs_until_stop);
    RUN_TEST(test_pins_slow_decay);
    RUN_TEST(test_pins_clamp_out_of_range);
    return UNITY_END();
}
