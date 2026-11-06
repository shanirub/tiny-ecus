#include "tiny_can.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "tiny_can";

// A frame and its data, kept together so the pointer handed to the driver
// stays valid until on_tx_done. `frame` is the first member, so the pointer
// the driver gives back is also the slot's address.
typedef struct {
    twai_frame_t frame;
    uint8_t data[TINY_CAN_MAX_LEN];
} tx_slot_t;

static twai_node_handle_t s_node;
static tx_slot_t *s_slots;
static QueueHandle_t s_free_slots;  // indices of unused TX slots
static QueueHandle_t s_rx;          // tiny_can_frame_t
static TaskHandle_t s_recover_task;

// Written only by the TWAI ISR (or, for tx_rejected, by tiny_can_send() under
// s_stats_lock); read as whole 32-bit words.
static volatile uint32_t s_tx_ok, s_tx_failed, s_rx_count, s_rx_dropped, s_bus_off;
static uint32_t s_tx_rejected;
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;

static bool on_tx_done(twai_node_handle_t node, const twai_tx_done_event_data_t *ev, void *ctx)
{
    (void)node;
    (void)ctx;
    if (ev->is_tx_success) {
        s_tx_ok++;
    } else {
        s_tx_failed++;
    }
    const uint8_t idx = (uint8_t)((const tx_slot_t *)ev->done_tx_frame - s_slots);
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_free_slots, &idx, &woken);
    return woken == pdTRUE;
}

static bool on_rx_done(twai_node_handle_t node, const twai_rx_done_event_data_t *ev, void *ctx)
{
    (void)ev;
    (void)ctx;
    tiny_can_frame_t f = {0};
    twai_frame_t rx = {.buffer = f.data, .buffer_len = sizeof f.data};
    if (twai_node_receive_from_isr(node, &rx) != ESP_OK) {
        return false;
    }
    f.id = rx.header.id;
    f.len = (uint8_t)(rx.header.dlc > TINY_CAN_MAX_LEN ? TINY_CAN_MAX_LEN : rx.header.dlc);
    BaseType_t woken = pdFALSE;
    if (xQueueSendFromISR(s_rx, &f, &woken) == pdTRUE) {
        s_rx_count++;
    } else {
        s_rx_dropped++;
    }
    return woken == pdTRUE;
}

static bool on_state_change(twai_node_handle_t node, const twai_state_change_event_data_t *ev, void *ctx)
{
    (void)node;
    (void)ctx;
    if (ev->new_sta != TWAI_ERROR_BUS_OFF) {
        return false;
    }
    s_bus_off++;
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_recover_task, &woken);
    return woken == pdTRUE;
}

static void recover_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(TINY_CAN_RECOVER_DELAY_MS));
        const esp_err_t err = twai_node_recover(s_node);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "recover: %s", esp_err_to_name(err));
        }
    }
}

esp_err_t tiny_can_start(const tiny_can_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(s_node == NULL, ESP_ERR_INVALID_STATE, TAG, "already started");
    ESP_RETURN_ON_FALSE(cfg && cfg->rx_queue_len > 0 && cfg->tx_queue_len > 0 && cfg->tx_queue_len < 255,
                        ESP_ERR_INVALID_ARG, TAG, "bad config");

    // One slot more than the driver's queue: the frame in the controller isn't
    // in the queue, so tiny_can_send() never blocks inside the driver.
    const uint32_t n_slots = cfg->tx_queue_len + 1;
    s_slots = calloc(n_slots, sizeof *s_slots);
    s_free_slots = xQueueCreate(n_slots, sizeof(uint8_t));
    s_rx = xQueueCreate(cfg->rx_queue_len, sizeof(tiny_can_frame_t));
    ESP_RETURN_ON_FALSE(s_slots && s_free_slots && s_rx, ESP_ERR_NO_MEM, TAG, "no memory");
    for (uint8_t i = 0; i < n_slots; i++) {
        xQueueSend(s_free_slots, &i, 0);
    }
    ESP_RETURN_ON_FALSE(xTaskCreate(recover_task, "can_recover", 2048, NULL, 5, &s_recover_task) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "no memory");

    const twai_onchip_node_config_t node_cfg = {
        .io_cfg = {
            .tx = cfg->tx_gpio,
            .rx = cfg->rx_gpio,
            .quanta_clk_out = -1,
            .bus_off_indicator = -1,
        },
        .bit_timing = {.bitrate = cfg->bitrate},
        .fail_retry_cnt = -1,  // retransmit until acknowledged, as CAN normally does
        .tx_queue_depth = cfg->tx_queue_len,
        .intr_priority = cfg->intr_priority,
        .flags = {
            .enable_self_test = cfg->self_test,
            .enable_loopback = cfg->self_test,
            .no_receive_rtr = true,
        },
    };
    ESP_RETURN_ON_ERROR(twai_new_node_onchip(&node_cfg, &s_node), TAG, "new node");

    const twai_mask_filter_config_t filter = {
        .id = cfg->filter_id & TWAI_STD_ID_MASK,
        .mask = cfg->filter_mask & TWAI_STD_ID_MASK,
        .is_ext = false,
    };
    ESP_RETURN_ON_ERROR(twai_node_config_mask_filter(s_node, 0, &filter), TAG, "filter");

    const twai_event_callbacks_t cbs = {
        .on_tx_done = on_tx_done,
        .on_rx_done = on_rx_done,
        .on_state_change = on_state_change,
    };
    ESP_RETURN_ON_ERROR(twai_node_register_event_callbacks(s_node, &cbs, NULL), TAG, "callbacks");
    ESP_RETURN_ON_ERROR(twai_node_enable(s_node), TAG, "enable");
    ESP_LOGI(TAG, "started: tx GPIO%d rx GPIO%d, %lu bit/s%s", cfg->tx_gpio, cfg->rx_gpio,
             (unsigned long)cfg->bitrate, cfg->self_test ? ", self-test" : "");
    return ESP_OK;
}

static void count_rejected(void)
{
    taskENTER_CRITICAL(&s_stats_lock);
    s_tx_rejected++;
    taskEXIT_CRITICAL(&s_stats_lock);
}

esp_err_t tiny_can_send(uint32_t id, const uint8_t *data, uint8_t len, uint32_t timeout_ms)
{
    if (s_node == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (id > TWAI_STD_ID_MASK || len > TINY_CAN_MAX_LEN || (len > 0 && data == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t idx;
    if (xQueueReceive(s_free_slots, &idx, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        count_rejected();
        return ESP_ERR_TIMEOUT;
    }
    tx_slot_t *slot = &s_slots[idx];
    if (len > 0) {
        memcpy(slot->data, data, len);
    }
    slot->frame = (twai_frame_t){
        .header = {.id = id, .dlc = len},
        .buffer = slot->data,
        .buffer_len = len,
    };
    const esp_err_t err = twai_node_transmit(s_node, &slot->frame, 0);
    if (err != ESP_OK) {
        xQueueSend(s_free_slots, &idx, 0);
        count_rejected();
    }
    return err;
}

bool tiny_can_receive(tiny_can_frame_t *out, uint32_t timeout_ms)
{
    return s_rx != NULL && xQueueReceive(s_rx, out, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void tiny_can_get_stats(tiny_can_stats_t *out)
{
    memset(out, 0, sizeof *out);
    if (s_node == NULL) {
        out->state = TINY_CAN_BUS_OFF;
        return;
    }
    twai_node_status_t st;
    twai_node_record_t rec;
    twai_node_get_info(s_node, &st, &rec);
    out->state = (tiny_can_state_t)st.state;  // same order as twai_error_state_t
    out->tec = st.tx_error_count;
    out->rec = st.rx_error_count;
    out->tx_ok = s_tx_ok;
    out->tx_failed = s_tx_failed;
    taskENTER_CRITICAL(&s_stats_lock);
    out->tx_rejected = s_tx_rejected;
    taskEXIT_CRITICAL(&s_stats_lock);
    out->rx = s_rx_count;
    out->rx_dropped = s_rx_dropped;
    out->bus_errors = rec.bus_err_num;
    out->bus_off = s_bus_off;
}

const char *tiny_can_state_name(tiny_can_state_t state)
{
    switch (state) {
    case TINY_CAN_ERROR_ACTIVE: return "active";
    case TINY_CAN_ERROR_WARNING: return "warning";
    case TINY_CAN_ERROR_PASSIVE: return "passive";
    case TINY_CAN_BUS_OFF: return "bus-off";
    }
    return "?";
}
