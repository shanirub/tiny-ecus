#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "tiny_proto.h"
#include "unity.h"

// The frame examples from components/tiny_proto/README.md, which a Python test
// also checks against the DBC. Raw values: rpm in 0.1 rpm units.

void setUp(void) {}

void tearDown(void) {}

static void test_ids_and_lengths(void)
{
    TEST_ASSERT_EQUAL_HEX32(0x100, TINY_PROTO_SETPOINT_0_FRAME_ID);
    TEST_ASSERT_EQUAL_HEX32(0x180, TINY_PROTO_STATUS_0_FRAME_ID);
    TEST_ASSERT_EQUAL(3, TINY_PROTO_SETPOINT_0_LENGTH);
    TEST_ASSERT_EQUAL(7, TINY_PROTO_STATUS_0_LENGTH);
    TEST_ASSERT_EQUAL(0, TINY_PROTO_SETPOINT_0_IS_EXTENDED);
    TEST_ASSERT_EQUAL(0, TINY_PROTO_STATUS_0_IS_EXTENDED);
}

static void test_setpoint_example_forward(void)
{
    const struct tiny_proto_setpoint_0_t msg = {.target_rpm = 550, .counter = 53};
    const uint8_t expected[] = {0x26, 0x02, 0x35};
    uint8_t buf[3];
    TEST_ASSERT_EQUAL(3, tiny_proto_setpoint_0_pack(buf, &msg, sizeof buf));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, 3);
}

static void test_setpoint_example_reverse(void)
{
    const struct tiny_proto_setpoint_0_t msg = {.target_rpm = -550, .counter = 53};
    const uint8_t expected[] = {0xDA, 0xFD, 0x35};
    uint8_t buf[3];
    TEST_ASSERT_EQUAL(3, tiny_proto_setpoint_0_pack(buf, &msg, sizeof buf));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, 3);
}

static void test_setpoint_unpack_example(void)
{
    const uint8_t frame[] = {0xDA, 0xFD, 0x35};
    struct tiny_proto_setpoint_0_t msg;
    TEST_ASSERT_EQUAL(0, tiny_proto_setpoint_0_unpack(&msg, frame, sizeof frame));
    TEST_ASSERT_EQUAL_INT16(-550, msg.target_rpm);
    TEST_ASSERT_EQUAL_UINT8(53, msg.counter);
}

static void test_status_example(void)
{
    const struct tiny_proto_status_0_t msg = {
        .rpm = 543,
        .duty = 41,
        .mode = TINY_PROTO_STATUS_0_MODE_RUN_CHOICE,
        .fault_stall = 0,
        .fault_timeout = 0,
        .counter = 122,
        .setpoint_seen = 53,
    };
    const uint8_t expected[] = {0x1F, 0x02, 0x29, 0x01, 0x00, 0x7A, 0x35};
    uint8_t buf[7];
    TEST_ASSERT_EQUAL(7, tiny_proto_status_0_pack(buf, &msg, sizeof buf));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, 7);
}

static void test_status_round_trip_extremes(void)
{
    const struct tiny_proto_status_0_t cases[] = {
        {.rpm = INT16_MIN, .duty = -100, .mode = 0, .fault_stall = 1, .fault_timeout = 0, .counter = 0, .setpoint_seen = 255},
        {.rpm = INT16_MAX, .duty = 100, .mode = 2, .fault_stall = 0, .fault_timeout = 1, .counter = 255, .setpoint_seen = 0},
        {.rpm = -1, .duty = -1, .mode = 1, .fault_stall = 1, .fault_timeout = 1, .counter = 128, .setpoint_seen = 127},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        uint8_t buf[7];
        struct tiny_proto_status_0_t back;
        TEST_ASSERT_EQUAL(7, tiny_proto_status_0_pack(buf, &cases[i], sizeof buf));
        TEST_ASSERT_EQUAL(0, tiny_proto_status_0_unpack(&back, buf, sizeof buf));
        TEST_ASSERT_EQUAL_INT16(cases[i].rpm, back.rpm);
        TEST_ASSERT_EQUAL_INT8(cases[i].duty, back.duty);
        TEST_ASSERT_EQUAL_UINT8(cases[i].mode, back.mode);
        TEST_ASSERT_EQUAL_UINT8(cases[i].fault_stall, back.fault_stall);
        TEST_ASSERT_EQUAL_UINT8(cases[i].fault_timeout, back.fault_timeout);
        TEST_ASSERT_EQUAL_UINT8(cases[i].counter, back.counter);
        TEST_ASSERT_EQUAL_UINT8(cases[i].setpoint_seen, back.setpoint_seen);
    }
}

static void test_setpoint_round_trip_limits(void)
{
    const int16_t rpms[] = {-1000, -1, 0, 1, 1000};
    for (size_t i = 0; i < sizeof rpms / sizeof rpms[0]; i++) {
        const struct tiny_proto_setpoint_0_t msg = {.target_rpm = rpms[i], .counter = (uint8_t)(250 + i)};
        uint8_t buf[3];
        struct tiny_proto_setpoint_0_t back;
        tiny_proto_setpoint_0_pack(buf, &msg, sizeof buf);
        TEST_ASSERT_EQUAL(0, tiny_proto_setpoint_0_unpack(&back, buf, sizeof buf));
        TEST_ASSERT_EQUAL_INT16(rpms[i], back.target_rpm);
        TEST_ASSERT_EQUAL_UINT8(msg.counter, back.counter);
    }
}

// Byte 4 is a bit field: stall in bit 0, timeout in bit 1, the rest 0.
static void test_fault_bits_positions(void)
{
    struct tiny_proto_status_0_t msg = {0};
    uint8_t buf[7];

    msg.fault_stall = 1;
    tiny_proto_status_0_pack(buf, &msg, sizeof buf);
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[4]);

    msg.fault_stall = 0;
    msg.fault_timeout = 1;
    tiny_proto_status_0_pack(buf, &msg, sizeof buf);
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[4]);

    msg.fault_stall = 1;
    tiny_proto_status_0_pack(buf, &msg, sizeof buf);
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[4]);
}

// A one-bit field given a larger value must not spill into its neighbor.
static void test_fault_bit_value_masked(void)
{
    const struct tiny_proto_status_0_t msg = {.fault_stall = 2};
    uint8_t buf[7];
    tiny_proto_status_0_pack(buf, &msg, sizeof buf);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[4]);
}

// Reserved bits 2..7 of byte 4 are ignored on receive.
static void test_reserved_fault_bits_ignored(void)
{
    const uint8_t frame[] = {0, 0, 0, 0, 0xFC, 0, 0};
    struct tiny_proto_status_0_t msg;
    TEST_ASSERT_EQUAL(0, tiny_proto_status_0_unpack(&msg, frame, sizeof frame));
    TEST_ASSERT_EQUAL_UINT8(0, msg.fault_stall);
    TEST_ASSERT_EQUAL_UINT8(0, msg.fault_timeout);
}

// Pack writes exactly the frame's length, nothing past it.
static void test_pack_stays_in_its_bytes(void)
{
    const struct tiny_proto_setpoint_0_t sp = {.target_rpm = -1, .counter = 255};
    const struct tiny_proto_status_0_t st = {.rpm = -1, .duty = -1, .mode = 2, .fault_stall = 1,
                                             .fault_timeout = 1, .counter = 255, .setpoint_seen = 255};
    uint8_t buf[8];

    memset(buf, 0xAA, sizeof buf);
    tiny_proto_setpoint_0_pack(buf, &sp, sizeof buf);
    for (size_t i = 3; i < sizeof buf; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xAA, buf[i]);
    }

    memset(buf, 0xAA, sizeof buf);
    tiny_proto_status_0_pack(buf, &st, sizeof buf);
    TEST_ASSERT_EQUAL_HEX8(0xAA, buf[7]);
}

static void test_short_buffers_rejected(void)
{
    const struct tiny_proto_setpoint_0_t sp = {.target_rpm = 550, .counter = 53};
    const struct tiny_proto_status_0_t st = {0};
    uint8_t buf[8];
    struct tiny_proto_setpoint_0_t sp_out;
    struct tiny_proto_status_0_t st_out;

    memset(buf, 0xAA, sizeof buf);
    TEST_ASSERT_EQUAL(-EINVAL, tiny_proto_setpoint_0_pack(buf, &sp, 2));
    TEST_ASSERT_EQUAL_HEX8(0xAA, buf[0]);
    TEST_ASSERT_EQUAL(-EINVAL, tiny_proto_status_0_pack(buf, &st, 6));
    TEST_ASSERT_EQUAL(-EINVAL, tiny_proto_setpoint_0_unpack(&sp_out, buf, 2));
    TEST_ASSERT_EQUAL(-EINVAL, tiny_proto_status_0_unpack(&st_out, buf, 6));
}

// The DBC's [min|max] as raw values: ±100.0 rpm is ±1000 raw.
static void test_range_helpers(void)
{
    TEST_ASSERT_TRUE(tiny_proto_setpoint_0_target_rpm_is_in_range(1000));
    TEST_ASSERT_TRUE(tiny_proto_setpoint_0_target_rpm_is_in_range(-1000));
    TEST_ASSERT_FALSE(tiny_proto_setpoint_0_target_rpm_is_in_range(1001));
    TEST_ASSERT_FALSE(tiny_proto_setpoint_0_target_rpm_is_in_range(-1001));
    TEST_ASSERT_TRUE(tiny_proto_status_0_duty_is_in_range(100));
    TEST_ASSERT_FALSE(tiny_proto_status_0_duty_is_in_range(101));
    TEST_ASSERT_FALSE(tiny_proto_status_0_duty_is_in_range(-101));
    TEST_ASSERT_TRUE(tiny_proto_status_0_mode_is_in_range(TINY_PROTO_STATUS_0_MODE_FAULT_CHOICE));
    TEST_ASSERT_FALSE(tiny_proto_status_0_mode_is_in_range(3));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ids_and_lengths);
    RUN_TEST(test_setpoint_example_forward);
    RUN_TEST(test_setpoint_example_reverse);
    RUN_TEST(test_setpoint_unpack_example);
    RUN_TEST(test_status_example);
    RUN_TEST(test_status_round_trip_extremes);
    RUN_TEST(test_setpoint_round_trip_limits);
    RUN_TEST(test_fault_bits_positions);
    RUN_TEST(test_fault_bit_value_masked);
    RUN_TEST(test_reserved_fault_bits_ignored);
    RUN_TEST(test_pack_stays_in_its_bytes);
    RUN_TEST(test_short_buffers_rejected);
    RUN_TEST(test_range_helpers);
    return UNITY_END();
}
