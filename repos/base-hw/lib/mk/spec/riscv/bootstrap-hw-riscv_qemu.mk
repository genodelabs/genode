INC_DIR += $(REP_DIR)/src/bootstrap/board/riscv_qemu

SRC_CC  += bootstrap/spec/riscv/platform.cc
SRC_S   += bootstrap/spec/riscv/crt0.s
SRC_CC  += lib/base/riscv/kernel/interface.cc
SRC_CC  += spec/64bit/memory_map.cc

vpath spec/64bit/memory_map.cc $(REP_DIR)/src/lib/hw

include $(REP_DIR)/lib/mk/bootstrap-hw.inc
