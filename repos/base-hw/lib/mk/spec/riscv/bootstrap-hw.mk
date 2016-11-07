INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/riscv

SRC_CC  += bootstrap/spec/riscv/cpu.cc
SRC_CC  += bootstrap/spec/riscv/exception_vector.cc
SRC_CC  += bootstrap/spec/riscv/platform.cc
SRC_CC  += lib/base/riscv/kernel/interface.cc
SRC_S   += bootstrap/spec/riscv/crt0.s
SRC_S   += core/spec/riscv/mode_transition.s

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc

