INC_DIR += $(REP_DIR)/src/core/spec/riscv

CC_OPT += -fno-delete-null-pointer-checks

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += kernel/vm_thread_off.cc kernel/kernel.cc
SRC_CC += spec/riscv/cpu.cc
SRC_CC += spec/riscv/kernel/thread.cc
SRC_CC += spec/riscv/kernel/cpu.cc
SRC_CC += spec/riscv/platform_support.cc
SRC_CC += spec/riscv/timer.cc
SRC_CC += spec/64bit/memory_map.cc

#add assembly sources
SRC_S += spec/riscv/exception_vector.s
SRC_S += spec/riscv/crt0.s

vpath spec/64bit/memory_map.cc $(BASE_DIR)/../base-hw/src/lib/hw

# include less specific configuration
include $(REP_DIR)/lib/mk/core-hw.inc
