SPECS += arm_v7

REP_INC_DIR += include/spec/arm_v7a

CC_MARCH += -march=armv7-a

include $(BASE_DIR)/mk/spec/arm_v7.mk

