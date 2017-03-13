include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avutil.mk

LIBAVUTIL_DIR = $(call select_from_ports,libav)/src/lib/libav/libavutil

-include $(LIBAVUTIL_DIR)/Makefile

vpath % $(LIBAVUTIL_DIR)
