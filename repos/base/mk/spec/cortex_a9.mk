SPECS += arm_v7a

REP_INC_DIR += include/spec/cortex_a9

CC_MARCH += -mcpu=cortex-a9

include $(BASE_DIR)/mk/spec/arm_v7a.mk

