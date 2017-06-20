INC_DIR += $(BASE_DIR)/../base-hw/src/bootstrap/spec/riscv

SRC_CC  += bootstrap/spec/riscv/platform.cc
SRC_CC  += lib/base/riscv/kernel/interface.cc
SRC_CC  += spec/64bit/memory_map.cc
SRC_S   += bootstrap/spec/riscv/crt0.s

vpath spec/64bit/memory_map.cc $(BASE_DIR)/../base-hw/src/lib/hw

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc
