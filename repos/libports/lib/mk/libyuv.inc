SHARED_LIB = yes

YUV_PORT = $(call select_from_ports,libyuv)
YUV_DIR  = $(YUV_PORT)/src/lib/libyuv

LIBS = libc stdcxx jpeg

INC_DIR += $(YUV_PORT)/include

SRC_CC = $(notdir $(wildcard $(YUV_DIR)/source/*.cc))

CC_CXX_WARN_STRICT = -Wextra -Werror
CC_OPT += -Wno-unused-parameter -DHAVE_JPEG

vpath %.cc $(YUV_DIR)/source
