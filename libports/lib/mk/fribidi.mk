include $(REP_DIR)/ports/fribidi.inc

FRIBIDI_DIR = $(REP_DIR)/contrib/$(FRIBIDI)

LIBS    += libc
INC_DIR += $(FRIBIDI_DIR) $(REP_DIR)/src/lib/fribidi $(REP_DIR)/include/fribidi
SRC_C    = $(notdir $(wildcard $(FRIBIDI_DIR)/lib/*.c))

CC_OPT  += -DHAVE_CONFIG_H
CC_WARN  =

vpath %.c $(FRIBIDI_DIR)/lib

SHARED_LIB = yes
