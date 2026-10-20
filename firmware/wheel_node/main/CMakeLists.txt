idf_component_register(SRCS "main.c" "console.c" "telemetry.c"
                       PRIV_REQUIRES tiny_common tiny_cmd esp_driver_gpio
                                     esp_driver_usb_serial_jtag vfs esp_timer esp_app_format)
