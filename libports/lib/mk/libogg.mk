include $(REP_DIR)/ports/libogg.inc
LIBOGG_DIR = $(REP_DIR)/contrib/$(LIBOGG)/src

SRC_C = $(notdir $(wildcard $(LIBOGG_DIR)/*.c))

vpath %.c $(LIBOGG_DIR)

INC_DIR += $(LIBOGG_DIR)

LIBS += libc

SHARED_LIB = yes
