#include "telemetry.h"

#include <stdatomic.h>
#include <stdio.h>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Lines start with "T," so tools/plot_run.py can tell them apart from command
// replies (OK/ERR) and IDF log lines on the same USB port.
#define TELEM_HEADER       "T,t_ms,sine_milli\n"
#define QUEUE_LEN          64    // 0.64 s of samples at 100 Hz
#define WRITER_STACK       3072
#define WRITER_PRIORITY    2     // below the console and the sampling/control task

static QueueHandle_t s_queue;
static atomic_bool s_enabled;
static atomic_bool s_header_pending;
static atomic_uint_fast32_t s_dropped;

// Printing can block (up to ~50 ms per stall in the USB VFS driver) when the
// host stops reading. Only this task prints samples, so a stalled host never
// delays the producer: if this task falls behind, the queue fills and samples
// are dropped and counted instead. Note that with no reader at all, the driver
// itself discards bytes after one 50 ms wait, so this task keeps up and that
// loss is not counted (bench check 2026-10-20: telemetry on, port closed for
// 5 s, console still answered at once, drops=0).
static void writer_task(void *arg)
{
    telemetry_sample_t s;
    for (;;) {
        if (xQueueReceive(s_queue, &s, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!atomic_load(&s_enabled)) {
            continue;  // switched off while queued
        }
        if (atomic_exchange(&s_header_pending, false)) {
            fputs(TELEM_HEADER, stdout);
        }
        printf("T,%lu,%ld\n", (unsigned long)s.t_ms, (long)s.sine_milli);
        fflush(stdout);
    }
}

void telemetry_start(void)
{
    s_queue = xQueueCreate(QUEUE_LEN, sizeof(telemetry_sample_t));
    ESP_ERROR_CHECK(s_queue ? ESP_OK : ESP_ERR_NO_MEM);
    BaseType_t ok = xTaskCreate(writer_task, "telem", WRITER_STACK, NULL, WRITER_PRIORITY, NULL);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

void telemetry_push(const telemetry_sample_t *s)
{
    if (!atomic_load(&s_enabled)) {
        return;
    }
    if (xQueueSend(s_queue, s, 0) != pdTRUE) {
        atomic_fetch_add(&s_dropped, 1);
    }
}

void telemetry_set_enabled(bool on)
{
    if (on && !atomic_load(&s_enabled)) {
        atomic_store(&s_header_pending, true);
    }
    atomic_store(&s_enabled, on);
}

bool telemetry_enabled(void)
{
    return atomic_load(&s_enabled);
}

uint32_t telemetry_dropped(void)
{
    return (uint32_t)atomic_load(&s_dropped);
}
