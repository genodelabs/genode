CC_C_OPT += -DARCH_X86=1

include $(REP_DIR)/lib/mk/avcodec.inc

include $(LIBAVCODEC_DIR)/x86/Makefile
