INC_DIR += $(BASE_DIR)/../base-hw/src/bootstrap/spec/rpi

SRC_CC  += bootstrap/spec/rpi/platform.cc
SRC_CC  += hw/spec/arm/arm_v6_cpu.cc
SRC_CC  += hw/spec/32bit/memory_map.cc
SRC_S   += bootstrap/spec/arm/crt0.s

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc
