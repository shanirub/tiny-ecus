idf_component_register(SRCS "main.c" "can_link.c" "commands.c"
                       PRIV_REQUIRES tiny_common tiny_can tiny_cmd tiny_console tiny_proto
                                     esp_driver_gpio esp_timer esp_app_format)
