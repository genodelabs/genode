YUV_DIR = $(call select_from_ports,libyuv)/libyuv

LIBS = libc

INC_DIR += $(YUV_DIR)/include

SRC_CC = $(notdir $(wildcard $(YUV_DIR)/source/*.cc))

CC_CXX_WARN_STRICT = -Wextra -Werror
CC_OPT += -Wno-unused-parameter

vpath %.cc $(YUV_DIR)/source
