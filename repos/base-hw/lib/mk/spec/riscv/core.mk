INC_DIR += $(REP_DIR)/src/core/include/spec/riscv

CC_OPT += -fno-delete-null-pointer-checks -msoft-float

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += kernel/vm_thread.cc kernel/kernel.cc
SRC_CC += spec/riscv/kernel/cpu_context.cc
SRC_CC += spec/riscv/kernel/thread.cc
SRC_CC += spec/riscv/kernel/pd.cc
SRC_CC += spec/riscv/kernel/cpu.cc
SRC_CC += spec/riscv/kernel/exception_vector.cc
SRC_CC += spec/riscv/platform_support.cc
SRC_CC += spec/riscv/cpu.cc

#add assembly sources
SRC_S += spec/riscv/mode_transition.s
SRC_S += spec/riscv/kernel/crt0.s
SRC_S += spec/riscv/crt0.s

# core.inc files use BASE_HW_DIR in order to allow
# including these files from other repositories
BASE_HW_DIR := $(REP_DIR)

# include less specific configuration
include $(REP_DIR)/lib/mk/core.inc
