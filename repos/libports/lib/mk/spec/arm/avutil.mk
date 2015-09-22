# must be defined before the inclusion of the libavutil 'Makefile'
ARCH_ARM=yes

CC_C_OPT += -DARCH_ARM=1

include $(REP_DIR)/lib/mk/avutil.inc
