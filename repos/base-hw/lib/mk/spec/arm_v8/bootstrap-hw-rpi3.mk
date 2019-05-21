INC_DIR += $(BASE_DIR)/../base-hw/src/bootstrap/spec/rpi3

SRC_CC  += lib/base/arm_64/kernel/interface.cc
SRC_CC  += spec/64bit/memory_map.cc
SRC_CC  += bootstrap/spec/rpi3/platform.cc
SRC_S   += bootstrap/spec/arm_64/crt0.s

vpath spec/64bit/memory_map.cc $(BASE_DIR)/../base-hw/src/lib/hw

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc
