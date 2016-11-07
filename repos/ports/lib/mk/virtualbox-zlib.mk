include $(REP_DIR)/lib/mk/virtualbox-common.inc

ZLIB_DIR = $(VIRTUALBOX_DIR)/src/libs/zlib-1.2.8
INC_DIR += $(ZLIB_DIR)
SRC_C    = $(notdir $(wildcard $(ZLIB_DIR)/*.c))

vpath % $(ZLIB_DIR)
