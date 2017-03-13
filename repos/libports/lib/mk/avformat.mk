include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avformat.mk

LIBAVFORMAT_DIR = $(call select_from_ports,libav)/src/lib/libav/libavformat

-include $(LIBAVFORMAT_DIR)/Makefile

LIBS  += avcodec avutil zlib

vpath % $(LIBAVFORMAT_DIR)
