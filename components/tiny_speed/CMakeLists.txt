# Hardware-free: no REQUIRES, so it also builds in test/host without ESP-IDF.
idf_component_register(SRCS "tiny_speed.c"
                       INCLUDE_DIRS "include")
