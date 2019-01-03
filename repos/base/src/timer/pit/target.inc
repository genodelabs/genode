TARGET   = pit_timer_drv
REQUIRES = x86
GEN_DIR := $(dir $(call select_from_repositories,src/timer/main.cc))
INC_DIR += $(GEN_DIR)/pit
SRC_CC  += time_source.cc

include $(GEN_DIR)/target.inc

vpath time_source.cc $(GEN_DIR)/pit
