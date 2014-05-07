include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avformat.mk

LIBAVFORMAT_DIR = $(REP_DIR)/contrib/$(LIBAV)/libavformat

include $(LIBAVFORMAT_DIR)/Makefile

LIBS  += avcodec avutil zlib

vpath % $(LIBAVFORMAT_DIR)
