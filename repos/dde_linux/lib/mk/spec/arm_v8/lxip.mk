SPEC_ARCH := arm_v8

SRC_C += lx_emul/shadow/arch/arm64/kernel/smp.c

#
# On arm64 the uaccess.h header uses __access_ok() prior to including
# our shadow header. Since this issue affects multiple complation units
# use a global compiler flag to point to the actual function we intend
# to be used.
#
CC_OPT += -D__access_ok=___access_ok

include $(REP_DIR)/lib/mk/lxip.inc
