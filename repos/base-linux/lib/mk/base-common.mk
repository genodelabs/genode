#
# \brief  Portions of base library shared by core and non-core processes
# \author Norman Feske
# \date   2013-02-14
#

LIBS += syscall

SRC_CC += ipc/ipc.cc
SRC_CC += avl_tree/avl_tree.cc
SRC_CC += allocator/slab.cc
SRC_CC += allocator/allocator_avl.cc
SRC_CC += heap/heap.cc heap/sliced_heap.cc
SRC_CC += console/console.cc
SRC_CC += child/child.cc
SRC_CC += process/process.cc
SRC_CC += elf/elf_binary.cc
SRC_CC += lock/lock.cc
SRC_CC += env/rm_session_mmap.cc env/debug.cc
SRC_CC += signal/signal.cc signal/common.cc
SRC_CC += server/server.cc server/common.cc
SRC_CC += thread/trace.cc thread/thread_env.cc thread/context_allocator.cc

INC_DIR +=  $(REP_DIR)/src/base/lock $(BASE_DIR)/src/base/lock
INC_DIR +=  $(REP_DIR)/src/base/ipc
INC_DIR +=  $(REP_DIR)/src/base/env $(BASE_DIR)/src/base/env
INC_DIR +=  $(REP_DIR)/src/platform $(BASE_DIR)/src/platform
INC_DIR += $(BASE_DIR)/src/base/thread

vpath %.cc $(REP_DIR)/src/base
vpath %.cc $(BASE_DIR)/src/base
