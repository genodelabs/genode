INC_DIR += $(REP_DIR)/src/bootstrap/spec/imx7d_sabre

SRC_CC  += bootstrap/spec/arm/cortex_a15_cpu.cc
SRC_CC  += bootstrap/spec/arm/gicv2.cc
SRC_CC  += bootstrap/spec/imx7d_sabre/platform.cc
SRC_CC  += bootstrap/spec/arm/arm_v7_cpu.cc
SRC_CC  += hw/spec/32bit/memory_map.cc
SRC_S   += bootstrap/spec/arm/crt0.s

NR_OF_CPUS = 2

#
# we need more specific compiler hints for some 'special' assembly code
# override -march=armv7-a because it conflicts with -mcpu=cortex-a7
#
CC_MARCH = -mcpu=cortex-a7 -mfpu=vfpv3 -mfloat-abi=softfp

include $(REP_DIR)/lib/mk/bootstrap-hw.inc
