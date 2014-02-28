LIBOGG_PORT_DIR := $(call select_from_ports,libogg)
LIBOGG_SRC_DIR = $(LIBOGG_PORT_DIR)/src/lib/libogg/src

INC_DIR += $(LIBOGG_PORT_DIR)/include

SRC_C = $(notdir $(wildcard $(LIBOGG_SRC_DIR)/*.c))

LIBS += libc

vpath %.c $(LIBOGG_SRC_DIR)

SHARED_LIB = yes
