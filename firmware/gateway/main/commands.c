#include "commands.h"

#include <inttypes.h>
#include <stdio.h>

#include "can_link.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "tiny_can.h"

static bool cmd_status(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    (void)argv;
    tiny_can_stats_t st;
    tiny_can_get_stats(&st);
    char age[16] = "none";
    uint32_t age_ms;
    if (can_link_status_age(&age_ms)) {
        snprintf(age, sizeof age, "%" PRIu32, age_ms);
    }
    snprintf(reply, size, "up_ms=%llu ver=%s heap=%lu can=%s status_age_ms=%s canmon=%s",
             (unsigned long long)(esp_timer_get_time() / 1000),
             esp_app_get_description()->version,
             (unsigned long)esp_get_free_heap_size(),
             tiny_can_state_name(st.state), age,
             can_link_monitor() ? "on" : "off");
    return true;
}

static bool cmd_can(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    (void)argv;
    tiny_can_stats_t st;
    tiny_can_get_stats(&st);
    snprintf(reply, size,
             "state=%s tec=%u rec=%u tx_ok=%" PRIu32 " tx_failed=%" PRIu32 " tx_rejected=%" PRIu32
             " rx=%" PRIu32 " rx_dropped=%" PRIu32 " bus_errors=%" PRIu32 " bus_off=%" PRIu32,
             tiny_can_state_name(st.state), st.tec, st.rec, st.tx_ok, st.tx_failed, st.tx_rejected,
             st.rx, st.rx_dropped, st.bus_errors, st.bus_off);
    return true;
}

static bool cmd_canmon(int argc, char *argv[], char *reply, size_t size)
{
    (void)argc;
    if (tiny_cmd_equals(argv[1], "ON")) {
        can_link_set_monitor(true);
    } else if (tiny_cmd_equals(argv[1], "OFF")) {
        can_link_set_monitor(false);
    } else {
        snprintf(reply, size, "usage: CANMON ON|OFF");
        return false;
    }
    snprintf(reply, size, "canmon %s", can_link_monitor() ? "on" : "off");
    return true;
}

static const tiny_cmd_t s_cmds[] = {
    {"STATUS", "STATUS", "uptime, firmware version, free heap, CAN state, ms since the last wheel status frame, monitor state",
     0, 0, cmd_status},
    {"CAN", "CAN", "CAN state, TEC/REC, frames sent/failed/rejected, received/dropped, bus errors, bus-off count",
     0, 0, cmd_can},
    {"CANMON", "CANMON ON|OFF", "print every received frame as C,<ms>,<id>,<len>,<data hex>",
     1, 1, cmd_canmon},
};

const tiny_cmd_table_t gateway_commands = {s_cmds, sizeof s_cmds / sizeof s_cmds[0]};
