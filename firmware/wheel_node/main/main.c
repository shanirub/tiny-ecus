#include <stdbool.h>

#include "console.h"
#include "drive.h"
#include "driver/gpio.h"
#include "encoder.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins.h"
#include "telemetry.h"
#include "tiny_common.h"

#define BLINK_HALF_PERIOD_MS  500   // 1 Hz blink: the firmware is alive

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

void app_main(void)
{
    tiny_common_print_banner("wheel node");

    // The drive task reads the encoder and sends telemetry, so both start
    // first; the console last, so no command arrives before the drive exists.
    encoder_start();
    telemetry_start();
    drive_start();
    console_start();

    BaseType_t ok = xTaskCreate(led_task, "led", 2048, NULL, 1, NULL);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
