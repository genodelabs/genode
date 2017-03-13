include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avfilter.mk

LIBAVFILTER_DIR = $(call select_from_ports,libav)/src/lib/libav/libavfilter

-include $(LIBAVFILTER_DIR)/Makefile

vpath % $(LIBAVFILTER_DIR)
