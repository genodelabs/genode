SHARED_LIB = yes

YUV_PORT := $(call select_from_ports,libyuv)
YUV_DIR   = $(YUV_PORT)/src/lib/libyuv

LIB_DIR   = $(REP_DIR)/src/lib/libyuv

LIBS = libc stdcxx jpeg

INC_DIR += $(REP_DIR)/include
INC_DIR += $(YUV_PORT)/include

SRC_CC  = $(notdir $(wildcard $(YUV_DIR)/source/*.cc))
SRC_CC += memory.cc

CC_CXX_WARN_STRICT = -Wextra -Werror
CC_OPT += -Wno-unused-parameter -DHAVE_JPEG

vpath memory.cc $(LIB_DIR)
vpath %.cc      $(YUV_DIR)/source
