SPECS += arm_v7a

REP_INC_DIR += include/spec/cortex_a8

CC_MARCH += -march=armv7-a -mcpu=cortex-a8

include $(BASE_DIR)/mk/spec/arm_v7a.mk
