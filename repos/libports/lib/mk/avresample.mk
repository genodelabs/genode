include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avresample.mk

LIBAVRESAMPLE_DIR = $(call select_from_ports,libav)/src/lib/libav/libavresample

-include $(LIBAVRESAMPLE_DIR)/Makefile

vpath % $(LIBAVRESAMPLE_DIR)

CC_CXX_WARN_STRICT =
