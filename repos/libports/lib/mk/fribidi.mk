FRIBIDI_PORT_DIR := $(call select_from_ports,fribidi)

FRIBIDI_DIR := $(FRIBIDI_PORT_DIR)/src/lib/fribidi/contrib

LIBS    += libc
INC_DIR += $(FRIBIDI_PORT_DIR)/include/fribidi \
           $(REP_DIR)/src/lib/fribidi \
           $(REP_DIR)/include/fribidi
SRC_C    = $(notdir $(wildcard $(FRIBIDI_DIR)/lib/*.c))

CC_OPT  += -DHAVE_CONFIG_H
CC_WARN  =

vpath %.c $(FRIBIDI_DIR)/lib

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
