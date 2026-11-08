#include <stdbool.h>

#include "can_link.h"
#include "commands.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tiny_common.h"
#include "tiny_console.h"

// ESP32-CAM small red LED: GPIO33, active-low (verified 2026-10-17). Not GPIO4, the bright flash LED.
// It faces the expansion board, so with the boards stacked only its glow is visible.
#define LED_GPIO        GPIO_NUM_33
#define LED_ACTIVE_LOW  1

#define BLINK_HALF_PERIOD_MS  500   // 1 Hz blink: the firmware is alive

static void led_set(bool on)
{
    gpio_set_level(LED_GPIO, LED_ACTIVE_LOW ? !on : on);
}

void app_main(void)
{
    tiny_common_print_banner("gateway");

    can_link_start();

    // UART0 at the IDF console's 115200; through the ESP32-CAM-MB's CH340 on /dev/ttyUSB0.
    const tiny_console_config_t console = {
        .transport = TINY_CONSOLE_UART0,
        .table = &gateway_commands,
        .tx_buffer = 2048,  // CANMON at 100 frames/s is ~3 kB/s, a quarter of the 11.5 kB/s line rate
        .rx_buffer = 256,
        .stack = 4096,
        .priority = 3,
    };
    ESP_ERROR_CHECK(tiny_console_start(&console));

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
