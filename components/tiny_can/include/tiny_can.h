#pragma once

// Thin layer over the ESP-IDF 6.1 TWAI node driver (esp_twai.h), shared by the
// firmware projects. One node per chip (both the ESP32 and the C3 have one TWAI
// controller), so the module keeps its state internally.
//
// What it adds to the driver:
// - TX buffers: the driver queues frames by pointer, so a frame must stay alive
//   until it has been sent. tiny_can_send() copies the data into a slot of an
//   internal pool, which on_tx_done hands back.
// - RX queue: the driver only gives out received frames inside its ISR callback;
//   that callback copies each frame into a FreeRTOS queue, read with
//   tiny_can_receive() from a task.
// - Bus-off recovery: after bus-off, a small task waits TINY_CAN_RECOVER_DELAY_MS
//   and starts the recovery (the controller then needs 128 x 11 recessive bits
//   before it rejoins). Bus-off events are counted.
// - Counters for diagnostics (tiny_can_get_stats()).
//
// Classic CAN only, 11-bit (standard) IDs, no remote frames.

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TINY_CAN_MAX_LEN           8
#define TINY_CAN_RECOVER_DELAY_MS  1000  // F.3: about 1 s between recovery attempts

typedef struct {
    int tx_gpio;            // to the transceiver's TX (D) pin
    int rx_gpio;            // from the transceiver's RX (R) pin
    uint32_t bitrate;       // bits/s, e.g. 500000
    int intr_priority;      // 1..3, or 0 to let the allocator pick one of 1..3
    bool self_test;         // no ACK needed and own frames are received back (bring-up alone on the bus)
    uint32_t filter_id;     // acceptance filter, 11-bit: a frame passes if (id & mask) == (filter_id & mask)
    uint32_t filter_mask;   // 0 accepts every standard ID
    uint32_t rx_queue_len;  // received frames buffered for tiny_can_receive()
    uint32_t tx_queue_len;  // frames waiting to be sent, besides the one in the controller
} tiny_can_config_t;

typedef struct {
    uint32_t id;                     // 11-bit ID
    uint8_t len;                     // data length (DLC), 0..8
    uint8_t data[TINY_CAN_MAX_LEN];
} tiny_can_frame_t;

typedef enum {
    TINY_CAN_ERROR_ACTIVE,   // normal: TEC and REC < 96
    TINY_CAN_ERROR_WARNING,  // TEC or REC >= 96
    TINY_CAN_ERROR_PASSIVE,  // TEC or REC >= 128: the node stays quieter about errors
    TINY_CAN_BUS_OFF,        // TEC >= 256: the node has disconnected itself
} tiny_can_state_t;

typedef struct {
    tiny_can_state_t state;
    uint16_t tec;          // transmit error counter
    uint16_t rec;          // receive error counter
    uint32_t tx_ok;        // frames sent and acknowledged
    uint32_t tx_failed;    // frames the controller gave up on
    uint32_t tx_rejected;  // tiny_can_send() calls that returned an error (no free slot, bus-off)
    uint32_t rx;           // frames received and queued
    uint32_t rx_dropped;   // frames received while the RX queue was full
    uint32_t bus_errors;   // bit, stuff, form, ACK errors and lost arbitrations seen by the controller
    uint32_t bus_off;      // times the node went bus-off
} tiny_can_stats_t;

/**
 * Create the node, set the filter and enable it. Call once; fails with
 * ESP_ERR_INVALID_STATE on a second call.
 */
esp_err_t tiny_can_start(const tiny_can_config_t *cfg);

/**
 * Queue one frame for sending. Waits up to `timeout_ms` for a free TX slot.
 * Returns ESP_ERR_INVALID_ARG for a bad ID or length, ESP_ERR_TIMEOUT if no
 * slot became free, ESP_ERR_INVALID_STATE while bus-off. Not for ISRs.
 */
esp_err_t tiny_can_send(uint32_t id, const uint8_t *data, uint8_t len, uint32_t timeout_ms);

/** Take the next received frame, waiting up to `timeout_ms`. False on timeout. */
bool tiny_can_receive(tiny_can_frame_t *out, uint32_t timeout_ms);

/** A snapshot of the state, error counters and frame counters. */
void tiny_can_get_stats(tiny_can_stats_t *out);

/** "active", "warning", "passive" or "bus-off". */
const char *tiny_can_state_name(tiny_can_state_t state);

#ifdef __cplusplus
}
#endif
