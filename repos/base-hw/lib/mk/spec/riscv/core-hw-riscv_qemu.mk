REP_INC_DIR += src/core/spec/riscv src/core/board/riscv_qemu

CC_OPT += -fno-delete-null-pointer-checks

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += kernel/cpu_up.cc
SRC_CC += kernel/lock.cc
SRC_CC += spec/riscv/kernel/thread.cc
SRC_CC += spec/riscv/kernel/cpu.cc
SRC_CC += spec/riscv/kernel/interface.cc
SRC_CC += spec/riscv/kernel/pd.cc
SRC_CC += spec/riscv/cpu.cc
SRC_CC += spec/riscv/platform_support.cc
SRC_CC += spec/riscv/timer.cc
SRC_CC += spec/64bit/memory_map.cc

#add assembly sources
SRC_S += spec/riscv/exception_vector.s
SRC_S += spec/riscv/crt0.s

vpath spec/64bit/memory_map.cc $(call select_from_repositories,src/lib/hw)

# include less specific configuration
include $(call select_from_repositories,lib/mk/core-hw.inc)
