SPECS       += arm_v8 arm_64 64bit
REP_INC_DIR += include/spec/arm_v8
REP_INC_DIR += include/spec/arm_64

CC_MARCH    ?= -march=armv8-a

include $(BASE_DIR)/mk/spec/64bit.mk
