INC_DIR += $(REP_DIR)/src/bootstrap/spec/rpi

SRC_CC  += bootstrap/spec/rpi/platform.cc
SRC_CC  += bootstrap/spec/arm/arm_v6_cpu.cc
SRC_CC  += hw/spec/32bit/memory_map.cc
SRC_S   += bootstrap/spec/arm/crt0.s

include $(REP_DIR)/lib/mk/bootstrap-hw.inc
