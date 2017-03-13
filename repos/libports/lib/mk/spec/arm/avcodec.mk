CC_C_OPT += -DARCH_ARM=1

include $(REP_DIR)/lib/mk/avcodec.inc

-include $(LIBAVCODEC_DIR)/arm/Makefile
