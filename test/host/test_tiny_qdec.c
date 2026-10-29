#include <stdint.h>
#include <stdio.h>

#include "tiny_qdec.h"
#include "unity.h"

// The forward ring as recorded in Phase 0 F.4, states written AB:
// 10 -> 11 -> 01 -> 00 -> 10. Written out here independently of the
// decoder's table, so the tests don't just copy it.
static const uint8_t RING[4] = {2, 3, 1, 0};

static int ring_index(uint8_t state)
{
    for (int i = 0; i < 4; i++) {
        if (RING[i] == state) {
            return i;
        }
    }
    return -1;
}

static tiny_qdec_t s_q;

// Feed `steps` single steps around the ring, forwards (> 0) or backwards (< 0).
static void turn(long steps)
{
    int i = ring_index(s_q.state);
    int dir = steps > 0 ? 1 : 3;  // +3 mod 4 is one step back
    long n = steps > 0 ? steps : -steps;
    for (long k = 0; k < n; k++) {
        i = (i + dir) % 4;
        tiny_qdec_update(&s_q, RING[i]);
    }
}

void setUp(void)
{
    tiny_qdec_init(&s_q, tiny_qdec_state(true, false));  // AB = 10
}

void tearDown(void) {}

static void test_state_packs_a_high_b_low(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, tiny_qdec_state(false, false));
    TEST_ASSERT_EQUAL_UINT8(1, tiny_qdec_state(false, true));
    TEST_ASSERT_EQUAL_UINT8(2, tiny_qdec_state(true, false));
    TEST_ASSERT_EQUAL_UINT8(3, tiny_qdec_state(true, true));
}

// All 16 (prev, now) pairs against the ring.
static void test_every_transition(void)
{
    for (uint8_t prev = 0; prev < 4; prev++) {
        for (uint8_t now = 0; now < 4; now++) {
            int ip = ring_index(prev);
            int in = ring_index(now);
            int expected;
            if (in == ip) {
                expected = 0;
            } else if (in == (ip + 1) % 4) {
                expected = +1;
            } else if (in == (ip + 3) % 4) {
                expected = -1;
            } else {
                expected = TINY_QDEC_INVALID;  // opposite side: both bits changed
            }
            char msg[32];
            snprintf(msg, sizeof msg, "prev=%u now=%u", prev, now);
            TEST_ASSERT_EQUAL_INT_MESSAGE(expected, tiny_qdec_step(prev, now), msg);
        }
    }
}

static void test_invalid_means_both_bits_changed(void)
{
    for (uint8_t s = 0; s < 4; s++) {
        TEST_ASSERT_EQUAL_INT(TINY_QDEC_INVALID, tiny_qdec_step(s, s ^ 3u));
    }
}

// tiny_qdec_step() is public, so it masks its own inputs instead of relying on
// tiny_qdec_update() having done it. Without the mask these index far past the
// table (a sanitizer error).
static void test_step_uses_only_low_two_bits(void)
{
    TEST_ASSERT_EQUAL_INT(tiny_qdec_step(3, 3), tiny_qdec_step(0xF3, 0xFF));
    TEST_ASSERT_EQUAL_INT(tiny_qdec_step(0, 2), tiny_qdec_step(0xFC, 0xFE));
}

static void test_init_starts_at_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, s_q.position);
    TEST_ASSERT_EQUAL_UINT32(0, s_q.invalid);
    TEST_ASSERT_EQUAL_UINT8(2, s_q.state);
}

static void test_forward_cycle_counts_four(void)
{
    turn(4);
    TEST_ASSERT_EQUAL_INT32(4, s_q.position);
    TEST_ASSERT_EQUAL_UINT8(2, s_q.state);  // back at the start state
}

static void test_backward_cycle_counts_minus_four(void)
{
    turn(-4);
    TEST_ASSERT_EQUAL_INT32(-4, s_q.position);
}

static void test_there_and_back_is_zero(void)
{
    turn(4320);
    turn(-4320);
    TEST_ASSERT_EQUAL_INT32(0, s_q.position);
    TEST_ASSERT_EQUAL_UINT32(0, s_q.invalid);
}

// C.3 bench check in numbers: 10 wheel turns of 48 x 90 counts.
static void test_ten_wheel_turns(void)
{
    turn(10L * 48 * 90);
    TEST_ASSERT_EQUAL_INT32(43200, s_q.position);
}

static void test_repeated_state_does_not_count(void)
{
    tiny_qdec_update(&s_q, 2);
    tiny_qdec_update(&s_q, 2);
    TEST_ASSERT_EQUAL_INT32(0, s_q.position);
    TEST_ASSERT_EQUAL_UINT32(0, s_q.invalid);
}

static void test_invalid_counts_and_keeps_position(void)
{
    turn(3);                       // 10 -> 11 -> 01 -> 00
    tiny_qdec_update(&s_q, 3);     // 00 -> 11: both changed
    TEST_ASSERT_EQUAL_INT32(3, s_q.position);
    TEST_ASSERT_EQUAL_UINT32(1, s_q.invalid);
}

static void test_resyncs_after_invalid(void)
{
    tiny_qdec_update(&s_q, 1);     // 10 -> 01: invalid
    turn(1);                       // 01 -> 00 counts from the new state
    TEST_ASSERT_EQUAL_INT32(1, s_q.position);
    TEST_ASSERT_EQUAL_UINT32(1, s_q.invalid);
}

static void test_only_low_two_bits_used(void)
{
    tiny_qdec_update(&s_q, 0xF3);  // read as 11: one step forward from 10
    TEST_ASSERT_EQUAL_INT32(1, s_q.position);
    TEST_ASSERT_EQUAL_UINT8(3, s_q.state);
}

static void test_zero_clears_counts_keeps_state(void)
{
    turn(4);                                 // back at 10
    tiny_qdec_update(&s_q, s_q.state ^ 3u);  // 10 -> 01: invalid
    // State 01 is neither 0 nor setUp's 10, so a zero that reset the state to
    // either would show here.
    uint8_t before = s_q.state;
    tiny_qdec_zero(&s_q);
    TEST_ASSERT_EQUAL_INT32(0, s_q.position);
    TEST_ASSERT_EQUAL_UINT32(0, s_q.invalid);
    TEST_ASSERT_EQUAL_UINT8(before, s_q.state);
    turn(1);  // counts normally from the kept state
    TEST_ASSERT_EQUAL_INT32(1, s_q.position);
}

// The position wraps instead of overflowing (signed overflow would be undefined
// behavior, which UBSan reports), and differences across the wrap stay right.
static void test_position_wraps(void)
{
    s_q.position = INT32_MAX;
    turn(1);
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, s_q.position);
    turn(-2);
    TEST_ASSERT_EQUAL_INT32(INT32_MAX - 1, s_q.position);
    int32_t before = s_q.position;
    turn(5);
    TEST_ASSERT_EQUAL_INT32(5, (int32_t)((uint32_t)s_q.position - (uint32_t)before));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_state_packs_a_high_b_low);
    RUN_TEST(test_every_transition);
    RUN_TEST(test_invalid_means_both_bits_changed);
    RUN_TEST(test_step_uses_only_low_two_bits);
    RUN_TEST(test_init_starts_at_zero);
    RUN_TEST(test_forward_cycle_counts_four);
    RUN_TEST(test_backward_cycle_counts_minus_four);
    RUN_TEST(test_there_and_back_is_zero);
    RUN_TEST(test_ten_wheel_turns);
    RUN_TEST(test_repeated_state_does_not_count);
    RUN_TEST(test_invalid_counts_and_keeps_position);
    RUN_TEST(test_resyncs_after_invalid);
    RUN_TEST(test_only_low_two_bits_used);
    RUN_TEST(test_zero_clears_counts_keeps_state);
    RUN_TEST(test_position_wraps);
    return UNITY_END();
}
