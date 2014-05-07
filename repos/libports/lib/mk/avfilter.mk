include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avfilter.mk

LIBAVFILTER_DIR = $(REP_DIR)/contrib/$(LIBAV)/libavfilter

include $(LIBAVFILTER_DIR)/Makefile

vpath % $(LIBAVFILTER_DIR)
