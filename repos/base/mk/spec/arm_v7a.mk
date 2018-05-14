SPECS    += arm_v7
CC_MARCH ?= -march=armv7-a -mfpu=vfpv3 -mfloat-abi=softfp

include $(BASE_DIR)/mk/spec/arm_v7.mk

