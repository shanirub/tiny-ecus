#include <stdint.h>

#include "tiny_pi.h"
#include "unity.h"

// Same shape as the firmware's starting values.
static const tiny_pi_cfg_t CFG = {
    .kp = 6.0f,
    .ki = 20.0f,
    .ff = 7.3f,
    .ff_offset = 4.2f,
    .dt = 0.01f,
    .out_max = 800.0f,
};

static tiny_pi_t s_pi;

void setUp(void)
{
    tiny_pi_reset(&s_pi);
}

void tearDown(void) {}

// A simulated bench wheel, from the D.4 measurements: at steady state
// rpm = 0.137 * duty_permille - 4.2 (for |duty| above the friction), reached
// with a 60 ms time constant. `load` (rpm) slows it, like a finger on the wheel.
static float plant(float rpm, int32_t duty, float load)
{
    float target = 0.0f;
    if (duty > 0) {
        target = 0.137f * (float)duty - 4.2f;
        target = target < 0.0f ? 0.0f : target;
    } else if (duty < 0) {
        target = 0.137f * (float)duty + 4.2f;
        target = target > 0.0f ? 0.0f : target;
    }
    target -= target > 0.0f ? load : target < 0.0f ? -load : 0.0f;
    return rpm + (target - rpm) * (0.01f / 0.06f);
}

// Run the loop for `n` periods from `*rpm`; returns the last duty.
static int32_t run(const tiny_pi_cfg_t *cfg, float setpoint, float *rpm, float load, int n)
{
    int32_t duty = 0;
    for (int i = 0; i < n; i++) {
        duty = tiny_pi_step(&s_pi, cfg, setpoint, *rpm);
        *rpm = plant(*rpm, duty, load);
    }
    return duty;
}

// --- Feedforward ---

static void test_feedforward_follows_the_measured_line(void)
{
    TEST_ASSERT_EQUAL_FLOAT(7.3f * 64.2f, tiny_pi_feedforward(&CFG, 60.0f));
    TEST_ASSERT_EQUAL_FLOAT(-7.3f * 64.2f, tiny_pi_feedforward(&CFG, -60.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, tiny_pi_feedforward(&CFG, 0.0f));
}

static void test_feedforward_off_when_ff_is_zero(void)
{
    tiny_pi_cfg_t cfg = CFG;
    cfg.ff = 0.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, tiny_pi_feedforward(&cfg, 60.0f));
}

// --- One step ---

static void test_at_setpoint_output_is_feedforward(void)
{
    // 7.3 * 64.2 = 468.66 -> 469
    TEST_ASSERT_EQUAL_INT32(469, tiny_pi_step(&s_pi, &CFG, 60.0f, 60.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, s_pi.integral);
}

static void test_p_term_and_integral_of_one_step(void)
{
    tiny_pi_cfg_t cfg = CFG;
    cfg.ff = 0.0f;
    // error 10: P = 60, integral = 20 * 10 * 0.01 = 2
    TEST_ASSERT_EQUAL_INT32(62, tiny_pi_step(&s_pi, &cfg, 50.0f, 40.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, s_pi.integral);
    // same again: integral 4
    TEST_ASSERT_EQUAL_INT32(64, tiny_pi_step(&s_pi, &cfg, 50.0f, 40.0f));
}

static void test_negative_error_mirrors(void)
{
    tiny_pi_cfg_t cfg = CFG;
    cfg.ff = 0.0f;
    TEST_ASSERT_EQUAL_INT32(-62, tiny_pi_step(&s_pi, &cfg, -50.0f, -40.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -2.0f, s_pi.integral);
}

static void test_output_rounds_half_away_from_zero(void)
{
    tiny_pi_cfg_t cfg = CFG;
    cfg.ff = 0.0f;
    cfg.ki = 0.0f;
    cfg.kp = 0.5f;
    TEST_ASSERT_EQUAL_INT32(2, tiny_pi_step(&s_pi, &cfg, 3.0f, 0.0f));    // 1.5
    TEST_ASSERT_EQUAL_INT32(-2, tiny_pi_step(&s_pi, &cfg, -3.0f, 0.0f));  // -1.5
    TEST_ASSERT_EQUAL_INT32(1, tiny_pi_step(&s_pi, &cfg, 2.6f, 0.0f));    // 1.3
}

static void test_output_is_limited(void)
{
    TEST_ASSERT_EQUAL_INT32(800, tiny_pi_step(&s_pi, &CFG, 100.0f, 0.0f));
    tiny_pi_reset(&s_pi);
    TEST_ASSERT_EQUAL_INT32(-800, tiny_pi_step(&s_pi, &CFG, -100.0f, 0.0f));
}

static void test_reset_clears_integral(void)
{
    tiny_pi_step(&s_pi, &CFG, 50.0f, 0.0f);
    TEST_ASSERT_NOT_EQUAL(0, (int)(s_pi.integral * 1000));
    tiny_pi_reset(&s_pi);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, s_pi.integral);
}

// --- Anti-windup ---

static void test_integral_frozen_while_saturated(void)
{
    // A blocked wheel: the error stays at 100 and the output at the limit.
    for (int i = 0; i < 500; i++) {
        TEST_ASSERT_EQUAL_INT32(800, tiny_pi_step(&s_pi, &CFG, 100.0f, 0.0f));
    }
    // Feedforward 759.66 + P 600 alone is past the limit, so nothing was added.
    TEST_ASSERT_EQUAL_FLOAT(0.0f, s_pi.integral);
}

static void test_integral_still_unwinds_while_saturated(void)
{
    // Wound up positive, then the error turns negative while the output is
    // still past the limit: the integral is allowed to shrink.
    // Feedforward at 90 rpm is 687.66; with the integral at 300 the output is
    // past 800 either way.
    s_pi.integral = 300.0f;
    TEST_ASSERT_EQUAL_INT32(800, tiny_pi_step(&s_pi, &CFG, 90.0f, 85.0f));  // error +5: frozen
    TEST_ASSERT_EQUAL_FLOAT(300.0f, s_pi.integral);
    TEST_ASSERT_EQUAL_INT32(800, tiny_pi_step(&s_pi, &CFG, 90.0f, 95.0f));  // error -5: 956.66
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 299.0f, s_pi.integral);  // shrank by 20 * 5 * 0.01
}

static void test_integral_never_exceeds_limit(void)
{
    // Without feedforward and P the output only saturates once the integral
    // is at the limit; it must stop there.
    tiny_pi_cfg_t cfg = CFG;
    cfg.ff = 0.0f;
    cfg.kp = 0.0f;
    cfg.ki = 1000.0f;
    for (int i = 0; i < 500; i++) {
        tiny_pi_step(&s_pi, &cfg, 100.0f, 0.0f);
    }
    TEST_ASSERT_TRUE(s_pi.integral <= 800.0f);
    TEST_ASSERT_TRUE(s_pi.integral > 790.0f);
}

static void test_changing_ki_keeps_the_output(void)
{
    // SET KI mid-run: the integral is kept in permille, already multiplied by
    // ki, so a new ki changes how fast it grows from now on, not the duty now.
    tiny_pi_cfg_t cfg = CFG;
    float rpm = 0.0f;
    run(&cfg, 60.0f, &rpm, 10.0f, 300);  // under a 10 rpm drag: the integral carries it
    TEST_ASSERT_TRUE(s_pi.integral > 50.0f);
    tiny_pi_t saved = s_pi;
    int32_t old_ki = tiny_pi_step(&s_pi, &cfg, 60.0f, rpm);
    s_pi = saved;
    cfg.ki = 2.0f * CFG.ki;
    int32_t new_ki = tiny_pi_step(&s_pi, &cfg, 60.0f, rpm);
    TEST_ASSERT_INT32_WITHIN(1, old_ki, new_ki);
}

// Peak speed of a 2 s run towards `setpoint` from rest.
static float peak_from_rest(float setpoint)
{
    float rpm = 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < 200; i++) {
        int32_t duty = tiny_pi_step(&s_pi, &CFG, setpoint, rpm);
        rpm = plant(rpm, duty, 0.0f);
        peak = rpm > peak ? rpm : peak;
    }
    return peak;
}

static void test_recovers_from_block_without_big_overshoot(void)
{
    // Hold 60 rpm, block the wheel for 2 s, release: the speed comes back to
    // 60 and overshoots hardly more than a fresh start does. Without
    // anti-windup the integral would sit at the limit and the wheel would race
    // to full speed first.
    float fresh = peak_from_rest(60.0f);
    tiny_pi_reset(&s_pi);
    float rpm = 0.0f;
    run(&CFG, 60.0f, &rpm, 0.0f, 100);
    for (int i = 0; i < 200; i++) {
        tiny_pi_step(&s_pi, &CFG, 60.0f, 0.0f);
    }
    rpm = 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < 200; i++) {
        int32_t duty = tiny_pi_step(&s_pi, &CFG, 60.0f, rpm);
        rpm = plant(rpm, duty, 0.0f);
        peak = rpm > peak ? rpm : peak;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, rpm);
    TEST_ASSERT_TRUE(peak < fresh + 2.0f);
}

// --- Closed loop on the simulated wheel ---

static void test_reaches_setpoint(void)
{
    float rpm = 0.0f;
    run(&CFG, 60.0f, &rpm, 0.0f, 100);  // 1 s
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, rpm);
}

static void test_reaches_negative_setpoint(void)
{
    float rpm = 0.0f;
    run(&CFG, -60.0f, &rpm, 0.0f, 100);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -60.0f, rpm);
}

static void test_p_only_leaves_an_error_under_load(void)
{
    // The glossary's point: P alone can't hold the speed against a load.
    tiny_pi_cfg_t cfg = CFG;
    cfg.ki = 0.0f;
    float rpm = 0.0f;
    run(&cfg, 60.0f, &rpm, 10.0f, 200);
    TEST_ASSERT_TRUE(rpm < 59.0f);
    TEST_ASSERT_TRUE(rpm > 50.0f);
}

static void test_integral_holds_speed_under_load(void)
{
    float rpm = 0.0f;
    run(&CFG, 60.0f, &rpm, 10.0f, 300);  // 3 s with a 10 rpm drag
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, rpm);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_feedforward_follows_the_measured_line);
    RUN_TEST(test_feedforward_off_when_ff_is_zero);
    RUN_TEST(test_at_setpoint_output_is_feedforward);
    RUN_TEST(test_p_term_and_integral_of_one_step);
    RUN_TEST(test_negative_error_mirrors);
    RUN_TEST(test_output_rounds_half_away_from_zero);
    RUN_TEST(test_output_is_limited);
    RUN_TEST(test_reset_clears_integral);
    RUN_TEST(test_integral_frozen_while_saturated);
    RUN_TEST(test_integral_still_unwinds_while_saturated);
    RUN_TEST(test_integral_never_exceeds_limit);
    RUN_TEST(test_changing_ki_keeps_the_output);
    RUN_TEST(test_recovers_from_block_without_big_overshoot);
    RUN_TEST(test_reaches_setpoint);
    RUN_TEST(test_reaches_negative_setpoint);
    RUN_TEST(test_p_only_leaves_an_error_under_load);
    RUN_TEST(test_integral_holds_speed_under_load);
    return UNITY_END();
}
