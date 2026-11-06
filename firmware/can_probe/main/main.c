// can_probe: CAN bring-up app for both boards (Phase 2 item B).
//
// Sends a counter frame every CONFIG_CAN_PROBE_PERIOD_MS, prints every frame it
// receives, and once a second prints a status line. Print-only: the mode is
// chosen at build time (Kconfig) until tiny_console exists (item C).
//
// Output lines, one per event:
//   R,<id>,<len>,<data hex>          a received frame (not printed for the flood ID)
//   G,<id>,<expected>,<got>          a counter frame arrived out of order
//   S,state=..,tec=..,rec=..,...     status, once a second
//
// Counter frame: 4 bytes, the counter as uint32 little-endian.

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tiny_can.h"
#include "tiny_common.h"

#if CONFIG_IDF_TARGET_ESP32
// ESP32-CAM gateway: GPIO14 has a weak pull-up at reset, so TX idles recessive
// while the chip boots (findings.md, 2026-11-04).
#define CAN_TX_GPIO  14
#define CAN_RX_GPIO  13
#define ROLE         "can_probe (gateway board)"
#elif CONFIG_IDF_TARGET_ESP32C3
#define CAN_TX_GPIO  4
#define CAN_RX_GPIO  3
#define ROLE         "can_probe (wheel node board)"
#else
#error "can_probe: no CAN pins for this target"
#endif

#define CAN_BITRATE     500000
#define FLOOD_ID        0x7FF
#define MAX_PEERS       4
#define STATUS_EVERY_MS 1000

#ifdef CONFIG_CAN_PROBE_SELF_TEST
#define SELF_TEST  true
#else
#define SELF_TEST  false
#endif

static const char *TAG = "can_probe";

typedef struct {
    uint32_t id;
    uint32_t last;
    uint32_t frames;
} peer_t;

static peer_t s_peers[MAX_PEERS];
static uint32_t s_gaps;
static uint32_t s_flood_rx;
static uint32_t s_flood_rejected;

static uint32_t get_le32(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

// Counter frames from each sender must arrive as last + 1.
static void check_counter(const tiny_can_frame_t *f)
{
    if (f->len != 4) {
        return;
    }
    const uint32_t value = get_le32(f->data);
    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t *p = &s_peers[i];
        if (p->frames == 0) {  // first frame from this ID
            *p = (peer_t){.id = f->id, .last = value, .frames = 1};
            return;
        }
        if (p->id == f->id) {
            if (value != p->last + 1) {
                s_gaps++;
                printf("G,%03" PRIX32 ",%" PRIu32 ",%" PRIu32 "\n", f->id, p->last + 1, value);
            }
            p->last = value;
            p->frames++;
            return;
        }
    }
}

static void rx_task(void *arg)
{
    (void)arg;
    tiny_can_frame_t f;
    for (;;) {
        if (!tiny_can_receive(&f, 1000)) {
            continue;
        }
        if (f.id == FLOOD_ID) {
            s_flood_rx++;
            continue;
        }
        char hex[2 * TINY_CAN_MAX_LEN + 1] = "";
        for (int i = 0; i < f.len; i++) {
            snprintf(&hex[2 * i], 3, "%02X", f.data[i]);
        }
        printf("R,%03" PRIX32 ",%u,%s\n", f.id, f.len, hex);
        check_counter(&f);
    }
}

static void tx_task(void *arg)
{
    (void)arg;
    uint32_t counter = 0;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        const uint8_t data[4] = {counter, counter >> 8, counter >> 16, counter >> 24};
        if (tiny_can_send(CONFIG_CAN_PROBE_TX_ID, data, sizeof data, 0) == ESP_OK) {
            counter++;  // a frame that couldn't be queued keeps its number, so no false gap
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(CONFIG_CAN_PROBE_PERIOD_MS));
    }
}

// Paces CONFIG_CAN_PROBE_FLOOD_FPS frames per second in 1 ms steps; a frame
// that finds no free TX slot is counted and skipped.
static void flood_task(void *arg)
{
    (void)arg;
    const uint8_t data[8] = {0xA5, 0x5A, 0xA5, 0x5A, 0xA5, 0x5A, 0xA5, 0x5A};
    uint32_t due_x1000 = 0;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        due_x1000 += CONFIG_CAN_PROBE_FLOOD_FPS;
        while (due_x1000 >= 1000) {
            due_x1000 -= 1000;
            if (tiny_can_send(FLOOD_ID, data, sizeof data, 0) != ESP_OK) {
                s_flood_rejected++;
            }
        }
        vTaskDelayUntil(&wake, 1);
    }
}

void app_main(void)
{
    tiny_common_print_banner(ROLE);

    const tiny_can_config_t cfg = {
        .tx_gpio = CAN_TX_GPIO,
        .rx_gpio = CAN_RX_GPIO,
        .bitrate = CAN_BITRATE,
        .intr_priority = 1,
        .self_test = SELF_TEST,
        .filter_mask = 0,  // accept every standard ID
        .rx_queue_len = 32,
        .tx_queue_len = 16,
    };
    ESP_ERROR_CHECK(tiny_can_start(&cfg));
    ESP_LOGI(TAG, "mode=%s tx_id=0x%03X period=%d ms flood=%d frames/s",
             cfg.self_test ? "self-test" : "normal", CONFIG_CAN_PROBE_TX_ID,
             CONFIG_CAN_PROBE_PERIOD_MS, CONFIG_CAN_PROBE_FLOOD_FPS);

    xTaskCreate(rx_task, "can_rx", 4096, NULL, 6, NULL);
    if (CONFIG_CAN_PROBE_TX_ID != 0) {
        xTaskCreate(tx_task, "can_tx", 3072, NULL, 5, NULL);
    }
    if (CONFIG_CAN_PROBE_FLOOD_FPS > 0) {
        xTaskCreate(flood_task, "can_flood", 3072, NULL, 4, NULL);
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(STATUS_EVERY_MS));
        tiny_can_stats_t st;
        tiny_can_get_stats(&st);
        printf("S,state=%s,tec=%u,rec=%u,tx_ok=%" PRIu32 ",tx_failed=%" PRIu32 ",tx_rejected=%" PRIu32
               ",rx=%" PRIu32 ",rx_dropped=%" PRIu32 ",bus_errors=%" PRIu32 ",bus_off=%" PRIu32
               ",gaps=%" PRIu32 ",flood_rx=%" PRIu32 ",flood_rejected=%" PRIu32 "\n",
               tiny_can_state_name(st.state), st.tec, st.rec, st.tx_ok, st.tx_failed, st.tx_rejected,
               st.rx, st.rx_dropped, st.bus_errors, st.bus_off, s_gaps, s_flood_rx, s_flood_rejected);
    }
}
