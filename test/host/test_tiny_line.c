#include <string.h>

#include "tiny_cmd.h"
#include "unity.h"

static tiny_line_t s_line;

// Feed a string; return the result of its last byte.
static tiny_line_result_t feed(const char *s)
{
    tiny_line_result_t r = TINY_LINE_NONE;
    for (; *s; s++) {
        r = tiny_line_feed(&s_line, *s);
    }
    return r;
}

void setUp(void)
{
    tiny_line_init(&s_line);
}

void tearDown(void) {}

static void test_partial_line_is_not_ready(void)
{
    TEST_ASSERT_EQUAL(TINY_LINE_NONE, feed("STAT"));
}

static void test_lf_ends_a_line(void)
{
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("STATUS\n"));
    TEST_ASSERT_EQUAL_STRING("STATUS", s_line.buf);
}

static void test_cr_ends_a_line(void)
{
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("STATUS\r"));
    TEST_ASSERT_EQUAL_STRING("STATUS", s_line.buf);
}

static void test_crlf_is_one_line(void)
{
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("A\r"));
    TEST_ASSERT_EQUAL_STRING("A", s_line.buf);
    TEST_ASSERT_EQUAL(TINY_LINE_NONE, tiny_line_feed(&s_line, '\n'));  // no extra blank line
    TEST_ASSERT_EQUAL_STRING("A", s_line.buf);                         // line still intact
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("B\r"));
    TEST_ASSERT_EQUAL_STRING("B", s_line.buf);
}

static void test_lf_lf_gives_a_blank_line(void)
{
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("A\n"));
    TEST_ASSERT_EQUAL(TINY_LINE_READY, tiny_line_feed(&s_line, '\n'));
    TEST_ASSERT_EQUAL_STRING("", s_line.buf);
}

static void test_consecutive_lines(void)
{
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("ONE\n"));
    TEST_ASSERT_EQUAL_STRING("ONE", s_line.buf);
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("TWO\n"));
    TEST_ASSERT_EQUAL_STRING("TWO", s_line.buf);
}

static void test_backspace_deletes(void)
{
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("STATX\bUS\n"));
    TEST_ASSERT_EQUAL_STRING("STATUS", s_line.buf);
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("AB\x7f\x7f\x7f" "C\n"));  // DEL, and more than there is
    TEST_ASSERT_EQUAL_STRING("C", s_line.buf);
}

static void test_control_characters_are_dropped_tab_kept(void)
{
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("A\x1b[\x01" "B\tC\n"));
    TEST_ASSERT_EQUAL_STRING("A[B\tC", s_line.buf);
}

static void test_exactly_max_length_is_accepted(void)
{
    char text[TINY_LINE_MAX + 2];
    memset(text, 'a', TINY_LINE_MAX);
    text[TINY_LINE_MAX] = '\n';
    text[TINY_LINE_MAX + 1] = '\0';
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed(text));
    TEST_ASSERT_EQUAL_size_t(TINY_LINE_MAX, strlen(s_line.buf));
}

static void test_too_long_line_is_discarded_then_recovers(void)
{
    char text[TINY_LINE_MAX + 3];
    memset(text, 'a', TINY_LINE_MAX + 1);
    text[TINY_LINE_MAX + 1] = '\n';
    text[TINY_LINE_MAX + 2] = '\0';
    TEST_ASSERT_EQUAL(TINY_LINE_TOO_LONG, feed(text));
    TEST_ASSERT_EQUAL(TINY_LINE_READY, feed("PING\n"));
    TEST_ASSERT_EQUAL_STRING("PING", s_line.buf);
}

static void test_backspace_does_not_rescue_an_overflowed_line(void)
{
    char text[TINY_LINE_MAX + 2];
    memset(text, 'a', TINY_LINE_MAX + 1);
    text[TINY_LINE_MAX + 1] = '\0';
    feed(text);
    TEST_ASSERT_EQUAL(TINY_LINE_TOO_LONG, feed("\b\b\n"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_partial_line_is_not_ready);
    RUN_TEST(test_lf_ends_a_line);
    RUN_TEST(test_cr_ends_a_line);
    RUN_TEST(test_crlf_is_one_line);
    RUN_TEST(test_lf_lf_gives_a_blank_line);
    RUN_TEST(test_consecutive_lines);
    RUN_TEST(test_backspace_deletes);
    RUN_TEST(test_control_characters_are_dropped_tab_kept);
    RUN_TEST(test_exactly_max_length_is_accepted);
    RUN_TEST(test_too_long_line_is_discarded_then_recovers);
    RUN_TEST(test_backspace_does_not_rescue_an_overflowed_line);
    return UNITY_END();
}
