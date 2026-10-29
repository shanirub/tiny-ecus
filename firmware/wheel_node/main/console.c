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
#define USJ_TX_BUFFER     1024  // telemetry at 100 Hz is ~2 kB/s
#define USJ_RX_BUFFER     256

static const char *mode_name(tiny_motor_mode_t mode)
{
    return mode == TINY_MOTOR_RUN ? "run" : mode == TINY_MOTOR_FAULT ? "stall" : "idle";
}

static bool cmd_status(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    (void)argv;
    tiny_motor_mode_t mode;
    int32_t duty;
    drive_status(&mode, &duty);
    snprintf(reply, size, "up_ms=%llu ver=%s heap=%lu telem=%s drops=%lu drive=%s duty=%ld",
             (unsigned long long)(esp_timer_get_time() / 1000),
             esp_app_get_description()->version,
             (unsigned long)esp_get_free_heap_size(),
             telemetry_enabled() ? "on" : "off",
             (unsigned long)telemetry_dropped(),
             mode_name(mode), (long)duty);
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
    if (!drive_run((int32_t)duty, (uint32_t)seconds)) {
        snprintf(reply, size, "stall fault latched, STOP clears it");
        return false;
    }
    snprintf(reply, size, "duty=%ld for %ld s", duty, seconds);
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

static const tiny_cmd_t s_cmds[] = {
    {"STATUS", "STATUS", "uptime, firmware version, free heap, telemetry state and drops, drive mode and duty %",
     0, 0, cmd_status},
    {"TELEM", "TELEM ON|OFF", "stream telemetry as CSV lines starting with T,",
     1, 1, cmd_telem},
    {"ENC", "ENC [ZERO]", "encoder position (counts) and invalid transitions; ZERO clears both first",
     0, 1, cmd_enc},
    {"DUTY", "DUTY <-100..100> [seconds]", "open-loop run at a duty % (cap 80), default 5 s, max 30, then ramps to a stop",
     1, 2, cmd_duty},
    {"STOP", "STOP", "stop the motor at once (coast) and clear a stall fault",
     0, 0, cmd_stop},
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
