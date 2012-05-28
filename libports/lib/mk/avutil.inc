include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avutil.mk

LIBAVUTIL_DIR = $(REP_DIR)/contrib/$(LIBAV)/libavutil

include $(LIBAVUTIL_DIR)/Makefile

vpath % $(LIBAVUTIL_DIR)
