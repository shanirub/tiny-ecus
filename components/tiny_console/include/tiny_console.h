#pragma once

// Serial command console shared by the firmware projects. It installs the
// driver for the chosen transport, routes printf/ESP_LOG through it, and runs
// a task that assembles input lines (tiny_line) and executes them against the
// app's command table (tiny_cmd), printing exactly one OK/ERR line per command.
//
// Other tasks may print their own lines (telemetry, CAN monitor) with printf:
// each printf call is written whole, so lines don't mix as long as every line
// is printed by a single call.

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "tiny_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TINY_CONSOLE_USB_SERIAL_JTAG,  // the C3's native USB port (/dev/ttyACM*); needs CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
    TINY_CONSOLE_UART0,            // UART0 at the console baud rate, e.g. through the ESP32-CAM-MB's CH340
} tiny_console_transport_t;

typedef struct {
    tiny_console_transport_t transport;
    const tiny_cmd_table_t *table;  // must stay valid for the life of the console
    size_t tx_buffer;               // output bytes the driver buffers; printf blocks when it's full
    size_t rx_buffer;               // input bytes the driver buffers
    uint32_t stack;                 // console task stack, bytes
    unsigned priority;              // console task priority
} tiny_console_config_t;

/**
 * Install the transport's driver, route stdout through it and start the
 * console task. Call once; ESP_ERR_INVALID_STATE on a second call,
 * ESP_ERR_NOT_SUPPORTED for a transport the chip doesn't have.
 */
esp_err_t tiny_console_start(const tiny_console_config_t *cfg);

#ifdef __cplusplus
}
#endif
