#
# evaluate bbl_dir immediately, otherwise it won't recognize
# missing ports when checking library dependencies
#
BBL_DIR := $(call select_from_ports,bbl)/src/lib/bbl

INC_DIR += $(REP_DIR)/src/core/spec/riscv $(BBL_DIR)

CC_OPT += -fno-delete-null-pointer-checks

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += kernel/cpu_up.cc
SRC_CC += kernel/lock.cc
SRC_CC += spec/riscv/cpu.cc
SRC_CC += spec/riscv/kernel/thread.cc
SRC_CC += spec/riscv/kernel/cpu.cc
SRC_CC += spec/riscv/kernel/pd.cc
SRC_CC += spec/riscv/platform_support.cc
SRC_CC += spec/riscv/timer.cc
SRC_CC += spec/64bit/memory_map.cc

#add assembly sources
SRC_S += spec/riscv/exception_vector.s
SRC_S += spec/riscv/crt0.s

vpath spec/64bit/memory_map.cc $(REP_DIR)/src/lib/hw

# include less specific configuration
include $(REP_DIR)/lib/mk/core-hw.inc
