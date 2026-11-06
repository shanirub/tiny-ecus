# Hardware-free: no REQUIRES, so it also builds in test/host without ESP-IDF.
# tiny_proto.c and include/tiny_proto.h are generated from tiny_proto.dbc by generate.py.
idf_component_register(SRCS "tiny_proto.c"
                       INCLUDE_DIRS "include")
