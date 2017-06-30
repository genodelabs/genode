SPECS += arm

CC_MARCH += -march=armv6k

REP_INC_DIR += include/spec/arm_v6

include $(BASE_DIR)/mk/spec/arm.mk

