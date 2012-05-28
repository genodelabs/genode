include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avcodec.mk

LIBAVCODEC_DIR = $(REP_DIR)/contrib/$(LIBAV)/libavcodec

include $(LIBAVCODEC_DIR)/Makefile

LIBS  += zlib

vpath % $(LIBAVCODEC_DIR)
