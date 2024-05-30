TARGET   = linux_timer
GEN_DIR := $(call select_from_repositories,src/timer/periodic)/..
INC_DIR += $(GEN_DIR)/periodic
SRC_CC  += periodic/time_source.cc time_source.cc
LIBS    += syscall-linux

include $(GEN_DIR)/target.inc

vpath periodic/time_source.cc $(GEN_DIR)
