# Needs ESP-IDF (TWAI driver, FreeRTOS), so it isn't host-tested; it stays thin.
idf_component_register(SRCS "tiny_can.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_driver_twai log freertos)
