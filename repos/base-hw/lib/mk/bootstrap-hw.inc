HW_DIR   = $(BASE_DIR)/../base-hw

LIBS     = cxx

SRC_CC  += bootstrap/env.cc
SRC_CC  += bootstrap/init.cc
SRC_CC  += bootstrap/lock.cc
SRC_CC  += bootstrap/platform.cc
SRC_CC  += bootstrap/thread.cc
SRC_CC  += core/capability.cc
SRC_CC  += core/core_log.cc
SRC_CC  += core/default_log.cc
SRC_CC  += core/dump_alloc.cc
SRC_CC  += core/kernel_log.cc
SRC_CC  += lib/base/allocator_avl.cc
SRC_CC  += lib/base/avl_tree.cc
SRC_CC  += lib/base/console.cc
SRC_CC  += lib/base/elf_binary.cc
SRC_CC  += lib/base/heap.cc
SRC_CC  += lib/base/log.cc
SRC_CC  += lib/base/output.cc
SRC_CC  += lib/base/slab.cc
SRC_CC  += lib/base/sleep.cc
SRC_CC  += lib/base/sliced_heap.cc
SRC_CC  += lib/startup/_main.cc

INC_DIR += $(HW_DIR)/src/bootstrap/include
INC_DIR += $(HW_DIR)/src/core/include
INC_DIR += $(BASE_DIR)/src/core/include
INC_DIR += $(HW_DIR)/src/include
INC_DIR += $(BASE_DIR)/src/include

# configure multiprocessor mode
NR_OF_CPUS ?= 1
CC_OPT += -Wa,--defsym -Wa,NR_OF_CPUS=$(NR_OF_CPUS) -DNR_OF_CPUS=$(NR_OF_CPUS)

vpath lib/base/%    $(HW_DIR)/src
vpath lib/muen/%    $(HW_DIR)/src
vpath lib/base/%    $(BASE_DIR)/src
vpath lib/startup/% $(BASE_DIR)/src
vpath base/%        $(HW_DIR)/src
vpath core/%        $(HW_DIR)/src
vpath core/%        $(BASE_DIR)/src
vpath bootstrap/%   $(HW_DIR)/src
