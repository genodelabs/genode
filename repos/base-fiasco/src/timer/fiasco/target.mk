TARGET   = fiasco_timer
LIBS    += syscall-fiasco
GEN_DIR := $(dir $(call select_from_repositories,src/timer/main.cc))
INC_DIR += $(GEN_DIR)/periodic
SRC_CC  += periodic/time_source.cc fiasco/time_source.cc

vpath %.cc $(GEN_DIR)

include $(GEN_DIR)/target.inc
