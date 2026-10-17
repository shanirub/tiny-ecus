#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tiny_common.h"

// C3 Super Mini onboard LED: GPIO8, active-low (verified 2026-10-17).
#define LED_GPIO        GPIO_NUM_8
#define LED_ACTIVE_LOW  1

#define BLINK_HALF_PERIOD_MS  500   // 1 Hz blink
#define HEARTBEAT_EVERY_N     10    // log every 10 half-periods = 5 s

static const char *TAG = "wheel_node";

static void led_set(bool on)
{
    gpio_set_level(LED_GPIO, LED_ACTIVE_LOW ? !on : on);
}

void app_main(void)
{
    tiny_common_print_banner("wheel node");

    const gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&led_cfg));

    bool on = false;
    for (unsigned n = 0;; n++) {
        on = !on;
        led_set(on);
        if (n % HEARTBEAT_EVERY_N == 0) {
            ESP_LOGI(TAG, "heartbeat %u s", n * BLINK_HALF_PERIOD_MS / 1000);
        }
        vTaskDelay(pdMS_TO_TICKS(BLINK_HALF_PERIOD_MS));
    }
}
