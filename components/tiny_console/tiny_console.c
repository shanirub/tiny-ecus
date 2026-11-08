#include "tiny_console.h"

#include <stdbool.h>
#include <stdio.h>

#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

// The driver's VFS part is only built when the IDF console is on USB-Serial-JTAG
// (which implies the chip has it).
#define HAVE_USJ  CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED

#if HAVE_USJ
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#endif

typedef int (*read_fn_t)(uint8_t *buf, size_t size);

static const tiny_cmd_table_t *s_table;
static read_fn_t s_read;

// uart_read_bytes() waits until it has `size` bytes, unlike the USB-Serial-JTAG
// read, so wait for one byte and then take only what is already buffered.
static int read_uart0(uint8_t *buf, size_t size)
{
    int n = uart_read_bytes(UART_NUM_0, buf, 1, portMAX_DELAY);
    size_t more = 0;
    if (n == 1 && size > 1 && uart_get_buffered_data_len(UART_NUM_0, &more) == ESP_OK && more > 0) {
        int m = uart_read_bytes(UART_NUM_0, buf + 1, more < size - 1 ? more : size - 1, 0);
        n += m > 0 ? m : 0;
    }
    return n;
}

#if HAVE_USJ
static int read_usj(uint8_t *buf, size_t size)
{
    return usb_serial_jtag_read_bytes(buf, size, portMAX_DELAY);
}
#endif

static void console_task(void *arg)
{
    (void)arg;
    static tiny_line_t line;  // static: keeps ~100 bytes off the task stack
    char reply[TINY_CMD_REPLY_MAX];
    uint8_t buf[32];

    tiny_line_init(&line);
    for (;;) {
        int n = s_read(buf, sizeof buf);
        for (int i = 0; i < n; i++) {
            tiny_line_result_t r = tiny_line_feed(&line, (char)buf[i]);
            if (r == TINY_LINE_TOO_LONG) {
                puts("ERR line too long");
                fflush(stdout);
            } else if (r == TINY_LINE_READY &&
                       tiny_cmd_execute(s_table, line.buf, reply, sizeof reply) != TINY_CMD_EMPTY) {
                puts(reply);
                fflush(stdout);
            }
        }
    }
}

// Install the driver and route printf/ESP_LOG through it too, so output is
// interrupt-driven and shares one path with the console reads.
static esp_err_t install(const tiny_console_config_t *cfg)
{
    switch (cfg->transport) {
    case TINY_CONSOLE_UART0: {
        esp_err_t err = uart_driver_install(UART_NUM_0, (int)cfg->rx_buffer, (int)cfg->tx_buffer, 0, NULL, 0);
        if (err != ESP_OK) {
            return err;
        }
        uart_vfs_dev_use_driver(UART_NUM_0);
        s_read = read_uart0;
        return ESP_OK;
    }
    case TINY_CONSOLE_USB_SERIAL_JTAG: {
#if HAVE_USJ
        usb_serial_jtag_driver_config_t usj = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        usj.tx_buffer_size = cfg->tx_buffer;
        usj.rx_buffer_size = cfg->rx_buffer;
        esp_err_t err = usb_serial_jtag_driver_install(&usj);
        if (err != ESP_OK) {
            return err;
        }
        usb_serial_jtag_vfs_use_driver();
        s_read = read_usj;
        return ESP_OK;
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t tiny_console_start(const tiny_console_config_t *cfg)
{
    if (s_table != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (cfg == NULL || cfg->table == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = install(cfg);
    if (err != ESP_OK) {
        return err;
    }
    s_table = cfg->table;
    BaseType_t ok = xTaskCreate(console_task, "console", cfg->stack, NULL, cfg->priority, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
