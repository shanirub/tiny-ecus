#include "console.h"

#include <stdio.h>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "drive.h"
#include "encoder.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "telemetry.h"
#include "tiny_cmd.h"

#define CONSOLE_STACK     4096
#define CONSOLE_PRIORITY  3
#define USJ_TX_BUFFER     1024  // telemetry at 100 Hz is ~3 kB/s (rows of ~30 bytes)
#define USJ_RX_BUFFER     256

static const char *mode_name(tiny_motor_mode_t mode)
{
    return mode == TINY_MOTOR_RUN ? "run" : mode == TINY_MOTOR_FAULT ? "stall" : "idle";
}

static const char *decay_name(tiny_motor_decay_t decay)
{
    return decay == TINY_MOTOR_DECAY_FAST ? "fast" : "slow";
}

static bool cmd_status(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    (void)argv;
    tiny_motor_mode_t mode;
    int32_t duty;
    tiny_motor_decay_t decay;
    drive_status(&mode, &duty, &decay);
    snprintf(reply, size, "up_ms=%llu ver=%s heap=%lu telem=%s drops=%lu drive=%s duty=%ld decay=%s probe=%s",
             (unsigned long long)(esp_timer_get_time() / 1000),
             esp_app_get_description()->version,
             (unsigned long)esp_get_free_heap_size(),
             telemetry_enabled() ? "on" : "off",
             (unsigned long)telemetry_dropped(),
             mode_name(mode), (long)duty, decay_name(decay),
             encoder_probe() ? "on" : "off");
    return true;
}

static bool cmd_telem(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    if (tiny_cmd_equals(argv[1], "ON")) {
        telemetry_set_enabled(true);
    } else if (tiny_cmd_equals(argv[1], "OFF")) {
        telemetry_set_enabled(false);
    } else {
        snprintf(reply, size, "usage: TELEM ON|OFF");
        return false;
    }
    snprintf(reply, size, "telem %s", telemetry_enabled() ? "on" : "off");
    return true;
}

static bool cmd_enc(int argc, char *argv[], char *reply, size_t size)
{
    if (argc == 2) {
        if (!tiny_cmd_equals(argv[1], "ZERO")) {
            snprintf(reply, size, "usage: ENC [ZERO]");
            return false;
        }
        encoder_zero();
    }
    int32_t position;
    uint32_t invalid;
    encoder_read(&position, &invalid);
    snprintf(reply, size, "pos=%ld invalid=%lu", (long)position, (unsigned long)invalid);
    return true;
}

#define RUN_DEFAULT_S  5
#define RUN_MAX_S      30

static bool cmd_duty(int argc, char *argv[], char *reply, size_t size)
{
    long duty;
    long seconds = RUN_DEFAULT_S;
    if (!tiny_cmd_parse_long(argv[1], -100, 100, &duty) ||
        (argc == 3 && !tiny_cmd_parse_long(argv[2], 1, RUN_MAX_S, &seconds))) {
        snprintf(reply, size, "usage: DUTY <-100..100> [1..%d s]", RUN_MAX_S);
        return false;
    }
    int32_t run;
    if (!drive_run((int32_t)duty, (uint32_t)seconds, &run)) {
        snprintf(reply, size, "stall fault latched, STOP clears it");
        return false;
    }
    if (run != duty) {
        snprintf(reply, size, "duty=%ld for %ld s (capped from %ld)", (long)run, seconds, duty);
    } else {
        snprintf(reply, size, "duty=%ld for %ld s", duty, seconds);
    }
    return true;
}

#define SPEED_MAX_RPM  100  // about what the 80 % duty cap reaches (105 rpm, D.4.2)

static bool cmd_speed(int argc, char *argv[], char *reply, size_t size)
{
    long rpm;
    long seconds = RUN_DEFAULT_S;
    if (!tiny_cmd_parse_long(argv[1], -SPEED_MAX_RPM, SPEED_MAX_RPM, &rpm) ||
        (argc == 3 && !tiny_cmd_parse_long(argv[2], 1, RUN_MAX_S, &seconds))) {
        snprintf(reply, size, "usage: SPEED <-%d..%d> [1..%d s]", SPEED_MAX_RPM, SPEED_MAX_RPM, RUN_MAX_S);
        return false;
    }
    if (!drive_speed((int32_t)rpm, (uint32_t)seconds)) {
        snprintf(reply, size, "stall fault latched, STOP clears it");
        return false;
    }
    snprintf(reply, size, "speed=%ld rpm for %ld s", rpm, seconds);
    return true;
}

// Gains as fixed-point text with 3 decimals, without float printf (the C
// library's printf may be built without float support). SET takes at most
// GAIN_DECIMALS, so the reply shows exactly the value that was set.
#define GAIN_DECIMALS 3

static void format_gain(char *buf, size_t size, float v)
{
    long milli = (long)(v * 1000.0f + 0.5f);  // gains are never negative
    snprintf(buf, size, "%ld.%03ld", milli / 1000, milli % 1000);
}

static bool cmd_set(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    static const struct {
        const char *name;
        drive_gain_t gain;
        float max;
    } gains[] = {
        {"KP", DRIVE_GAIN_KP, 50.0f},
        {"KI", DRIVE_GAIN_KI, 500.0f},
        {"FF", DRIVE_GAIN_FF, 20.0f},
    };
    for (size_t i = 0; i < sizeof gains / sizeof gains[0]; i++) {
        if (!tiny_cmd_equals(argv[1], gains[i].name)) {
            continue;
        }
        float v;
        if (!tiny_cmd_parse_float(argv[2], GAIN_DECIMALS, 0.0f, gains[i].max, &v)) {
            char max[16];
            format_gain(max, sizeof max, gains[i].max);
            snprintf(reply, size, "%s must be 0..%s, at most %d decimals", gains[i].name, max, GAIN_DECIMALS);
            return false;
        }
        drive_set_gain(gains[i].gain, v);
        char text[16];
        format_gain(text, sizeof text, v);
        snprintf(reply, size, "%s=%s", gains[i].name, text);
        return true;
    }
    snprintf(reply, size, "usage: SET KP|KI|FF <value>");
    return false;
}

static bool cmd_gains(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    (void)argv;
    float kp, ki, ff;
    drive_gains(&kp, &ki, &ff);
    char a[16], b[16], c[16];
    format_gain(a, sizeof a, kp);
    format_gain(b, sizeof b, ki);
    format_gain(c, sizeof c, ff);
    snprintf(reply, size, "kp=%s ki=%s ff=%s", a, b, c);
    return true;
}

static bool cmd_stop(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    (void)argv;
    drive_stop();
    snprintf(reply, size, "stopped");
    return true;
}

static bool cmd_decay(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    tiny_motor_decay_t decay;
    if (tiny_cmd_equals(argv[1], "SLOW")) {
        decay = TINY_MOTOR_DECAY_SLOW;
    } else if (tiny_cmd_equals(argv[1], "FAST")) {
        decay = TINY_MOTOR_DECAY_FAST;
    } else {
        snprintf(reply, size, "usage: DECAY SLOW|FAST");
        return false;
    }
    if (!drive_set_decay(decay)) {
        snprintf(reply, size, "motor running, change decay when idle");
        return false;
    }
    snprintf(reply, size, "decay=%s", decay_name(decay));
    return true;
}

static bool cmd_probe(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    if (tiny_cmd_equals(argv[1], "ON")) {
        encoder_set_probe(true);
    } else if (tiny_cmd_equals(argv[1], "OFF")) {
        encoder_set_probe(false);
    } else {
        snprintf(reply, size, "usage: PROBE ON|OFF");
        return false;
    }
    snprintf(reply, size, "probe %s", encoder_probe() ? "on" : "off");
    return true;
}

static const tiny_cmd_t s_cmds[] = {
    {"STATUS", "STATUS", "uptime, firmware version, free heap, telemetry state and drops, drive mode, duty %, decay, probe",
     0, 0, cmd_status},
    {"TELEM", "TELEM ON|OFF", "stream telemetry as CSV lines starting with T,",
     1, 1, cmd_telem},
    {"ENC", "ENC [ZERO]", "encoder position (counts) and invalid transitions; ZERO clears both first",
     0, 1, cmd_enc},
    {"DUTY", "DUTY <-100..100> [seconds]", "open-loop run at a duty % (cap 80), default 5 s, max 30, then ramps to a stop",
     1, 2, cmd_duty},
    {"SPEED", "SPEED <-100..100> [seconds]", "speed-controlled run at wheel rpm, default 5 s, max 30, then ramps to a stop",
     1, 2, cmd_speed},
    {"SET", "SET KP|KI|FF <value>", "change a speed controller gain, also mid-run (see GAINS)",
     2, 2, cmd_set},
    {"GAINS", "GAINS", "speed controller gains: kp, ki (permille duty per rpm, per rpm*s), ff (permille per rpm)",
     0, 0, cmd_gains},
    {"STOP", "STOP", "stop the motor at once (coast) and clear a stall fault",
     0, 0, cmd_stop},
    {"DECAY", "DECAY SLOW|FAST", "PWM decay mode for the next runs (slow at boot); only while not running",
     1, 1, cmd_decay},
    {"PROBE", "PROBE ON|OFF", "GPIO1 high while the encoder interrupt handler runs, for the logic analyzer",
     1, 1, cmd_probe},
};

static const tiny_cmd_table_t s_table = {s_cmds, sizeof s_cmds / sizeof s_cmds[0]};

static void console_task(void *arg)
{
    static tiny_line_t line;  // static: keeps ~100 bytes off the task stack
    char reply[TINY_CMD_REPLY_MAX];
    uint8_t buf[32];

    tiny_line_init(&line);
    for (;;) {
        int n = usb_serial_jtag_read_bytes(buf, sizeof buf, portMAX_DELAY);
        for (int i = 0; i < n; i++) {
            tiny_line_result_t r = tiny_line_feed(&line, (char)buf[i]);
            if (r == TINY_LINE_TOO_LONG) {
                puts("ERR line too long");
                fflush(stdout);
            } else if (r == TINY_LINE_READY &&
                       tiny_cmd_execute(&s_table, line.buf, reply, sizeof reply) != TINY_CMD_EMPTY) {
                puts(reply);
                fflush(stdout);
            }
        }
    }
}

void console_start(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.tx_buffer_size = USJ_TX_BUFFER;
    cfg.rx_buffer_size = USJ_RX_BUFFER;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
    // Route printf/ESP_LOG through the driver too, so output is interrupt-driven
    // and shares one path with the console reads.
    usb_serial_jtag_vfs_use_driver();

    BaseType_t ok = xTaskCreate(console_task, "console", CONSOLE_STACK, NULL, CONSOLE_PRIORITY, NULL);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
