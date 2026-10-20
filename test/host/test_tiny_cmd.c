#include <stdio.h>
#include <string.h>

#include "tiny_cmd.h"
#include "unity.h"

// A fake handler that records how it was called.
static int s_calls;
static int s_argc;
static char s_argv1[32];
static bool s_result;

static bool fake_handler(int argc, char *argv[], char *reply, size_t size)
{
    s_calls++;
    s_argc = argc;
    snprintf(s_argv1, sizeof s_argv1, "%s", argc > 1 ? argv[1] : "");
    snprintf(reply, size, "got %d", argc - 1);
    return s_result;
}

static bool silent_handler(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    (void)argv;
    (void)reply;
    (void)size;
    return true;
}

static const tiny_cmd_t s_cmds[] = {
    {"SPEED", "SPEED <rpm>", "set the speed", 1, 1, fake_handler},
    {"PING", "PING", "answer OK", 0, 0, silent_handler},
    {"MULTI", "MULTI [a] [b]", "zero to two args", 0, 2, fake_handler},
};
static const tiny_cmd_table_t s_table = {s_cmds, sizeof s_cmds / sizeof s_cmds[0]};

static char s_reply[TINY_CMD_REPLY_MAX];

static tiny_cmd_status_t run(const char *text)
{
    char line[TINY_LINE_MAX + 1];
    snprintf(line, sizeof line, "%s", text);
    return tiny_cmd_execute(&s_table, line, s_reply, sizeof s_reply);
}

void setUp(void)
{
    s_calls = 0;
    s_argc = -1;
    s_argv1[0] = '\0';
    s_result = true;
    memset(s_reply, 'x', sizeof s_reply);  // catch a missing NUL terminator
}

void tearDown(void) {}

// --- dispatch -------------------------------------------------------------

static void test_calls_handler_with_argument(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_OK, run("SPEED 60"));
    TEST_ASSERT_EQUAL_INT(1, s_calls);
    TEST_ASSERT_EQUAL_INT(2, s_argc);
    TEST_ASSERT_EQUAL_STRING("60", s_argv1);
    TEST_ASSERT_EQUAL_STRING("OK got 1", s_reply);
}

static void test_handler_failure_gives_err(void)
{
    s_result = false;
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("SPEED 60"));
    TEST_ASSERT_EQUAL_STRING("ERR got 1", s_reply);
}

static void test_empty_detail_gives_bare_ok(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_OK, run("PING"));
    TEST_ASSERT_EQUAL_STRING("OK", s_reply);
}

static void test_command_is_case_insensitive(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_OK, run("speed 60"));
    TEST_ASSERT_EQUAL(TINY_CMD_OK, run("SpEeD 60"));
    TEST_ASSERT_EQUAL_INT(2, s_calls);
}

static void test_extra_whitespace_is_ignored(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_OK, run("  \tSPEED   60 \t "));
    TEST_ASSERT_EQUAL_STRING("60", s_argv1);
}

static void test_blank_line_is_empty_with_no_reply(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_EMPTY, run(""));
    TEST_ASSERT_EQUAL_STRING("", s_reply);
    TEST_ASSERT_EQUAL(TINY_CMD_EMPTY, run("   \t "));
    TEST_ASSERT_EQUAL_INT(0, s_calls);
}

static void test_unknown_command(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("FLY 3"));
    TEST_ASSERT_EQUAL_STRING("ERR unknown command 'FLY'", s_reply);
    TEST_ASSERT_EQUAL_INT(0, s_calls);
}

static void test_prefix_of_a_command_is_unknown(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("SPEE 60"));
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("SPEEDY 60"));
    TEST_ASSERT_EQUAL_INT(0, s_calls);
}

static void test_too_few_arguments_shows_usage(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("SPEED"));
    TEST_ASSERT_EQUAL_STRING("ERR usage: SPEED <rpm>", s_reply);
    TEST_ASSERT_EQUAL_INT(0, s_calls);
}

static void test_too_many_arguments_shows_usage(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("SPEED 60 70"));
    TEST_ASSERT_EQUAL_STRING("ERR usage: SPEED <rpm>", s_reply);
    TEST_ASSERT_EQUAL_INT(0, s_calls);
}

static void test_argument_range(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_OK, run("MULTI"));
    TEST_ASSERT_EQUAL(TINY_CMD_OK, run("MULTI a"));
    TEST_ASSERT_EQUAL(TINY_CMD_OK, run("MULTI a b"));
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("MULTI a b c"));
    TEST_ASSERT_EQUAL_INT(3, s_calls);
}

static void test_too_many_words(void)
{
    // TINY_CMD_MAX_TOKENS is 8: the command word plus 7 more is the limit.
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("MULTI 1 2 3 4 5 6 7 8"));
    TEST_ASSERT_EQUAL_STRING("ERR too many words", s_reply);
    // Exactly 8 words reaches the argument check instead.
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("MULTI 1 2 3 4 5 6 7"));
    TEST_ASSERT_EQUAL_STRING("ERR usage: MULTI [a] [b]", s_reply);
}

static void test_tiny_reply_buffer_is_truncated_safely(void)
{
    char line[] = "FLY";
    char small[6];
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, tiny_cmd_execute(&s_table, line, small, sizeof small));
    TEST_ASSERT_EQUAL_STRING("ERR u", small);
}

// --- HELP -----------------------------------------------------------------

static void test_help_lists_all_commands(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_OK, run("HELP"));
    TEST_ASSERT_EQUAL_STRING("OK HELP SPEED PING MULTI", s_reply);
}

static void test_help_for_one_command(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_OK, run("help speed"));
    TEST_ASSERT_EQUAL_STRING("OK SPEED <rpm>: set the speed", s_reply);
}

static void test_help_for_unknown_command(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("HELP FLY"));
    TEST_ASSERT_EQUAL_STRING("ERR unknown command 'FLY'", s_reply);
}

static void test_help_with_too_many_arguments(void)
{
    TEST_ASSERT_EQUAL(TINY_CMD_ERR, run("HELP SPEED PING"));
    TEST_ASSERT_EQUAL_STRING("ERR usage: HELP [command]", s_reply);
}

// --- tiny_cmd_equals ------------------------------------------------------

static void test_equals(void)
{
    TEST_ASSERT_TRUE(tiny_cmd_equals("on", "ON"));
    TEST_ASSERT_TRUE(tiny_cmd_equals("", ""));
    TEST_ASSERT_FALSE(tiny_cmd_equals("ON", "ONE"));
    TEST_ASSERT_FALSE(tiny_cmd_equals("ONE", "ON"));
    TEST_ASSERT_FALSE(tiny_cmd_equals("OFF", "ON"));
}

// --- number parsing -------------------------------------------------------

static void test_parse_long_valid(void)
{
    long v = 0;
    TEST_ASSERT_TRUE(tiny_cmd_parse_long("60", -100, 100, &v));
    TEST_ASSERT_EQUAL_INT32(60, v);
    TEST_ASSERT_TRUE(tiny_cmd_parse_long("-100", -100, 100, &v));
    TEST_ASSERT_EQUAL_INT32(-100, v);
    TEST_ASSERT_TRUE(tiny_cmd_parse_long("+100", -100, 100, &v));
    TEST_ASSERT_EQUAL_INT32(100, v);
}

static void test_parse_long_rejects_and_leaves_output(void)
{
    long v = 42;
    TEST_ASSERT_FALSE(tiny_cmd_parse_long("", -100, 100, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_long("-", -100, 100, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_long("60abc", -100, 100, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_long("6.5", -100, 100, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_long("0x10", -100, 100, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_long("101", -100, 100, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_long("-101", -100, 100, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_long("99999999999999999999999", -100, 100, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_long(NULL, -100, 100, &v));
    TEST_ASSERT_EQUAL_INT32(42, v);
}

static void test_parse_float_valid(void)
{
    float v = 0;
    TEST_ASSERT_TRUE(tiny_cmd_parse_float("0.25", 0, 10, &v));
    TEST_ASSERT_EQUAL_FLOAT(0.25f, v);
    TEST_ASSERT_TRUE(tiny_cmd_parse_float("-1.5", -2, 2, &v));
    TEST_ASSERT_EQUAL_FLOAT(-1.5f, v);
    TEST_ASSERT_TRUE(tiny_cmd_parse_float("2", 0, 10, &v));
    TEST_ASSERT_EQUAL_FLOAT(2.0f, v);
}

static void test_parse_float_rejects_and_leaves_output(void)
{
    float v = 7.0f;
    TEST_ASSERT_FALSE(tiny_cmd_parse_float("", 0, 10, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_float("1.5x", 0, 10, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_float("nan", 0, 10, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_float("inf", 0, 10, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_float("10.5", 0, 10, &v));
    TEST_ASSERT_FALSE(tiny_cmd_parse_float("1e40", 0, 10, &v));
    TEST_ASSERT_EQUAL_FLOAT(7.0f, v);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_calls_handler_with_argument);
    RUN_TEST(test_handler_failure_gives_err);
    RUN_TEST(test_empty_detail_gives_bare_ok);
    RUN_TEST(test_command_is_case_insensitive);
    RUN_TEST(test_extra_whitespace_is_ignored);
    RUN_TEST(test_blank_line_is_empty_with_no_reply);
    RUN_TEST(test_unknown_command);
    RUN_TEST(test_prefix_of_a_command_is_unknown);
    RUN_TEST(test_too_few_arguments_shows_usage);
    RUN_TEST(test_too_many_arguments_shows_usage);
    RUN_TEST(test_argument_range);
    RUN_TEST(test_too_many_words);
    RUN_TEST(test_tiny_reply_buffer_is_truncated_safely);
    RUN_TEST(test_help_lists_all_commands);
    RUN_TEST(test_help_for_one_command);
    RUN_TEST(test_help_for_unknown_command);
    RUN_TEST(test_help_with_too_many_arguments);
    RUN_TEST(test_equals);
    RUN_TEST(test_parse_long_valid);
    RUN_TEST(test_parse_long_rejects_and_leaves_output);
    RUN_TEST(test_parse_float_valid);
    RUN_TEST(test_parse_float_rejects_and_leaves_output);
    return UNITY_END();
}
