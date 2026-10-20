#include <math.h>
#include <stdbool.h>

#include "console.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "telemetry.h"
#include "tiny_common.h"

// C3 Super Mini onboard LED: GPIO8, active-low (verified 2026-10-17).
#define LED_GPIO        GPIO_NUM_8
#define LED_ACTIVE_LOW  1

#define BLINK_HALF_PERIOD_MS  500   // 1 Hz blink: the firmware is alive

// Stand-in for the item D/E control loop: same rate, same telemetry path.
#define SAMPLE_PERIOD_MS      10    // 100 Hz
#define SINE_PERIOD_MS        2000
#define SAMPLE_PRIORITY       5     // highest of our tasks, as the control loop will be

static void led_set(bool on)
{
    gpio_set_level(LED_GPIO, LED_ACTIVE_LOW ? !on : on);
}

static void led_task(void *arg)
{
    const gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&led_cfg));

    bool on = false;
    for (;;) {
        on = !on;
        led_set(on);
        vTaskDelay(pdMS_TO_TICKS(BLINK_HALF_PERIOD_MS));
    }
}

static void sample_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        // vTaskDelayUntil keeps a fixed period, unlike vTaskDelay, which adds
        // the loop's own run time to every period.
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
        uint32_t t_ms = (uint32_t)(esp_timer_get_time() / 1000);
        float phase = (float)(t_ms % SINE_PERIOD_MS) / SINE_PERIOD_MS;
        telemetry_sample_t s = {
            .t_ms = t_ms,
            .sine_milli = (int32_t)lroundf(1000.0f * sinf(2.0f * (float)M_PI * phase)),
        };
        telemetry_push(&s);
    }
}

void app_main(void)
{
    tiny_common_print_banner("wheel node");

    telemetry_start();
    console_start();

    BaseType_t ok = xTaskCreate(led_task, "led", 2048, NULL, 1, NULL);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ok = xTaskCreate(sample_task, "sample", 3072, NULL, SAMPLE_PRIORITY, NULL);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
