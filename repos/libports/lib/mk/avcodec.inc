include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avcodec.mk

LIBAV_PORT_DIR := $(call select_from_ports,libav)
LIBAVCODEC_DIR := $(LIBAV_PORT_DIR)/src/lib/libav/libavcodec

-include $(LIBAVCODEC_DIR)/Makefile

LIBS += zlib

INC_DIR += $(LIBAV_PORT_DIR)/src/lib/libav

vpath % $(LIBAVCODEC_DIR)
