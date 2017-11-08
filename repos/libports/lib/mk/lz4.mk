LZ4_PORT_DIR := $(call select_from_ports,lz4)
LZ4_DIR      := $(LZ4_PORT_DIR)/src/lib/lz4/lib

SRC_C = lz4.c \
        lz4frame.c \
        lz4hc.c \
        xxhash.c

vpath %.c $(LZ4_DIR)

INC_DIR += $(LZ4_DIR)

LIBS += libc

SHARED_LIB = yes
