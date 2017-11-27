# must be defined before the inclusion of the libavutil 'Makefile'
ARCH_X86=yes

CC_C_OPT += -DARCH_X86=1

include $(REP_DIR)/lib/mk/avutil.inc

INC_DIR += $(REP_DIR)/src/lib/libav

-include $(LIBAVUTIL_DIR)/x86/Makefile
