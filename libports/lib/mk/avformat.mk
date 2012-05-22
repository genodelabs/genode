include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avformat.mk

CC_WARN += -Wno-pointer-sign -Wno-deprecated-declarations

LIBAVFORMAT_DIR = $(REP_DIR)/contrib/$(LIBAV)/libavformat

include $(LIBAVFORMAT_DIR)/Makefile

LIBS  += avcodec avutil zlib

vpath % $(LIBAVFORMAT_DIR)
