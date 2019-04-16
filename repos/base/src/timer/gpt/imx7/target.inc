TARGET   = imx7_timer_drv
REQUIRES = arm_v7
GEN_DIR := $(BASE_DIR)/src/timer
INC_DIR += $(GEN_DIR)/gpt
SRC_CC  += gpt/time_source.cc gpt/imx7/timer.cc

include $(GEN_DIR)/target.inc

vpath %.cc $(GEN_DIR)
