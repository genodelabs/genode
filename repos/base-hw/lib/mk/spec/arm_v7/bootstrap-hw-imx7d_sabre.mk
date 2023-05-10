REP_INC_DIR += src/bootstrap/board/imx7d_sabre

SRC_CC  += bootstrap/spec/arm/cortex_a15_cpu.cc
SRC_CC  += bootstrap/spec/arm/gicv2.cc
SRC_CC  += bootstrap/board/imx7d_sabre/platform.cc

include $(call select_from_repositories,lib/mk/spec/arm_v7/bootstrap-hw.inc)

#
# we need more specific compiler hints for some 'special' assembly code
# override -march=armv7-a because it conflicts with -mcpu=cortex-a7
#
CC_MARCH = -mcpu=cortex-a7+nofp -mfpu=vfpv3
