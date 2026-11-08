# Needs ESP-IDF (UART / USB-Serial-JTAG drivers, FreeRTOS), so it isn't
# host-tested; the line and command logic it runs is tiny_cmd, which is.
idf_component_register(SRCS "tiny_console.c"
                       INCLUDE_DIRS "include"
                       REQUIRES tiny_cmd
                       PRIV_REQUIRES esp_driver_uart esp_driver_usb_serial_jtag vfs freertos)
