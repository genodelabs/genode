REQUIRES += sel4

SRC_CC += native_cpu.cc

vpath native_cpu.cc $(PRG_DIR)/../..

include $(PRG_DIR)/../../target.inc
