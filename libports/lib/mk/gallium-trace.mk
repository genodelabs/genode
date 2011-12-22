include $(REP_DIR)/lib/mk/gallium.inc

SRC_C := $(notdir $(wildcard $(GALLIUM_SRC_DIR)/drivers/trace/*.c))

vpath %.c $(GALLIUM_SRC_DIR)/drivers/trace
