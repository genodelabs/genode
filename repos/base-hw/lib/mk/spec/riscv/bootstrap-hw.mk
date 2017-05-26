INC_DIR += $(BASE_DIR)/../base-hw/src/bootstrap/spec/riscv

SRC_CC  += bootstrap/spec/riscv/platform.cc
SRC_CC  += lib/base/riscv/kernel/interface.cc
SRC_S   += bootstrap/spec/riscv/crt0.s

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc
