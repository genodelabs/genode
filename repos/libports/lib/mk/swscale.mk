include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-swscale.mk

CC_WARN += -Wno-switch

LIBSWSCALE_DIR = $(call select_from_ports,libav)/src/lib/libav/libswscale

-include $(LIBSWSCALE_DIR)/Makefile

vpath % $(LIBSWSCALE_DIR)
