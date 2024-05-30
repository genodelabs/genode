TARGET = intel_gpu
SRC_CC = main.cc mmio_dump.cc
LIBS   = base

REQUIRES = x86

INC_DIR += $(PRG_DIR)

CC_CXX_WARN_STRICT_CONVERSION =
