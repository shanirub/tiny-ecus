#include "can_link.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>

#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tiny_can.h"
#include "tiny_proto.h"

// ESP32-CAM: GPIO14 has a weak pull-up at reset, so TX idles recessive while
// the chip boots (findings.md, 2026-11-04).
#define CAN_TX_GPIO   14
#define CAN_RX_GPIO   13
#define CAN_BITRATE   500000

#define RX_STACK      3072
#define RX_PRIORITY   6

static atomic_bool s_monitor;
static atomic_bool s_status_seen;
static _Atomic uint32_t s_status_ms;  // uptime of the last status frame (32 bit: native atomics)

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void print_frame(const tiny_can_frame_t *f)
{
    char hex[2 * TINY_CAN_MAX_LEN + 1] = "";
    for (int i = 0; i < f->len; i++) {
        snprintf(&hex[2 * i], 3, "%02X", f->data[i]);
    }
    // One printf per line, so lines from other tasks don't cut into it.
    printf("C,%" PRIu32 ",%03" PRIX32 ",%u,%s\n", now_ms(), f->id, f->len, hex);
}

static void rx_task(void *arg)
{
    (void)arg;
    tiny_can_frame_t f;
    for (;;) {
        if (!tiny_can_receive(&f, 1000)) {
            continue;
        }
        if (f.id == TINY_PROTO_STATUS_0_FRAME_ID) {
            atomic_store(&s_status_ms, now_ms());
            atomic_store(&s_status_seen, true);
        }
        // printf blocks while the UART's TX buffer is full: at a high frame
        // rate the monitor slows this task down and the RX queue overflows
        // (counted as rx_dropped). Fine for a diagnostic.
        if (atomic_load(&s_monitor)) {
            print_frame(&f);
        }
    }
}

void can_link_start(void)
{
    const tiny_can_config_t cfg = {
        .tx_gpio = CAN_TX_GPIO,
        .rx_gpio = CAN_RX_GPIO,
        .bitrate = CAN_BITRATE,
        .intr_priority = 1,
        .filter_mask = 0,  // every standard ID, so the monitor shows the whole bus
        .rx_queue_len = 32,
        .tx_queue_len = 16,
    };
    ESP_ERROR_CHECK(tiny_can_start(&cfg));
    BaseType_t ok = xTaskCreate(rx_task, "can_rx", RX_STACK, NULL, RX_PRIORITY, NULL);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

bool can_link_status_age(uint32_t *age_ms)
{
    if (!atomic_load(&s_status_seen)) {
        return false;
    }
    *age_ms = now_ms() - atomic_load(&s_status_ms);  // unsigned: right across the 49-day wrap
    return true;
}

void can_link_set_monitor(bool on)
{
    atomic_store(&s_monitor, on);
}

bool can_link_monitor(void)
{
    return atomic_load(&s_monitor);
}
