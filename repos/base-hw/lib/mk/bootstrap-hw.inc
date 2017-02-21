HW_DIR   = $(BASE_DIR)/../base-hw

LIBS     = cxx

SRC_CC  += bootstrap/env.cc
SRC_CC  += bootstrap/init.cc
SRC_CC  += bootstrap/lock.cc
SRC_CC  += bootstrap/log.cc
SRC_CC  += bootstrap/platform.cc
SRC_CC  += bootstrap/thread.cc
SRC_CC  += hw/capability.cc
SRC_CC  += lib/base/allocator_avl.cc
SRC_CC  += lib/base/avl_tree.cc
SRC_CC  += lib/base/console.cc
SRC_CC  += lib/base/elf_binary.cc
SRC_CC  += lib/base/heap.cc
SRC_CC  += lib/base/registry.cc
SRC_CC  += lib/base/log.cc
SRC_CC  += lib/base/output.cc
SRC_CC  += lib/base/slab.cc
SRC_CC  += lib/base/sleep.cc
SRC_CC  += lib/base/sliced_heap.cc
SRC_CC  += lib/startup/_main.cc

INC_DIR += $(HW_DIR)/src/bootstrap
INC_DIR += $(HW_DIR)/src/lib
INC_DIR += $(HW_DIR)/src/include
INC_DIR += $(BASE_DIR)/src/include
INC_DIR += $(BASE_DIR)/src/core/include # for boot_modules.h only

# configure multiprocessor mode
NR_OF_CPUS ?= 1
CC_OPT += -Wa,--defsym -Wa,NR_OF_CPUS=$(NR_OF_CPUS) -DNR_OF_CPUS=$(NR_OF_CPUS)

vpath base/%        $(HW_DIR)/src
vpath bootstrap/%   $(HW_DIR)/src
vpath hw/%          $(HW_DIR)/src/lib
vpath lib/base/%    $(HW_DIR)/src
vpath lib/muen/%    $(HW_DIR)/src
vpath lib/base/%    $(BASE_DIR)/src
vpath lib/startup/% $(BASE_DIR)/src
