TARGET = intel_gpu_drv
SRC_CC = main.cc mmio_dump.cc
LIBS   = base

REQUIRES = x86_64

INC_DIR += $(PRG_DIR)
