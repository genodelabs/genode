TARGET = test-sel4
SRC_CC = main.cc context_area.cc mini_env.cc thread.cc

LIBS   = core_printf syscall

#
# Equivalent to base-common library, excluding some parts that would conflict
# with the minimalistic root-task environment (e.g., thread.cc)
#
LIBS += cxx startup

SRC_CC += ipc/ipc.cc ipc/pager.cc
SRC_CC += avl_tree/avl_tree.cc
SRC_CC += allocator/slab.cc
SRC_CC += allocator/allocator_avl.cc
SRC_CC += heap/heap.cc heap/sliced_heap.cc
SRC_CC += console/console.cc
SRC_CC += child/child.cc
SRC_CC += process/process.cc
SRC_CC += elf/elf_binary.cc
SRC_CC += lock/lock.cc
SRC_CC += signal/signal.cc signal/common.cc
SRC_CC += server/server.cc
SRC_CC += thread/trace.cc
SRC_CC += thread/context_allocator.cc
SRC_CC += env/capability.cc env/capability_space.cc

INC_DIR +=  $(REP_DIR)/src/base
INC_DIR +=  $(REP_DIR)/src/base/lock
INC_DIR += $(BASE_DIR)/src/base/lock
INC_DIR += $(BASE_DIR)/src/base/thread

vpath %.cc $(REP_DIR)/src/base
vpath %.cc $(BASE_DIR)/src/base

vpath %.cc $(BASE_DIR)/src

SRC_CC += thread_sel4.cc
vpath thread_sel4.cc $(REP_DIR)/src/base/thread

SRC_CC += thread_bootstrap.cc
vpath thread_bootstrap.cc $(REP_DIR)/src/base/thread
