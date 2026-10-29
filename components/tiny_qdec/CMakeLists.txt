# Hardware-free: no REQUIRES, so it also builds in test/host without ESP-IDF.
# linker.lf places its code and table in RAM, for the encoder ISR.
idf_component_register(SRCS "tiny_qdec.c"
                       INCLUDE_DIRS "include"
                       LDFRAGMENTS "linker.lf")
