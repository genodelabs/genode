include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-swscale.mk

CC_WARN += -Wno-switch

LIBSWSCALE_DIR = $(REP_DIR)/contrib/$(LIBAV)/libswscale

include $(LIBSWSCALE_DIR)/Makefile

vpath % $(LIBSWSCALE_DIR)
