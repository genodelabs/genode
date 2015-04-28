#
# \brief  Portions of base library shared by core and non-core processes
# \author Norman Feske
# \date   2013-02-14
#

LIBS += cxx startup

SRC_CC += cap_copy.cc
SRC_CC += ipc/ipc.cc ipc/pager.cc ipc/ipc_marshal_cap.cc
SRC_CC += pager/pager.cc pager/common.cc
SRC_CC += avl_tree/avl_tree.cc
SRC_CC += allocator/slab.cc
SRC_CC += allocator/allocator_avl.cc
SRC_CC += heap/heap.cc heap/sliced_heap.cc
SRC_CC += console/console.cc
SRC_CC += child/child.cc
SRC_CC += process/process.cc
SRC_CC += elf/elf_binary.cc
SRC_CC += lock/lock.cc
SRC_CC += signal/signal.cc signal/common.cc signal/platform.cc
SRC_CC += server/server.cc server/common.cc
SRC_CC += thread/thread.cc thread/thread_bootstrap.cc thread/trace.cc
SRC_CC += thread/myself.cc
SRC_CC += thread/context_allocator.cc

INC_DIR +=  $(REP_DIR)/src/base/lock
INC_DIR += $(BASE_DIR)/src/base/lock
INC_DIR += $(BASE_DIR)/src/base/thread

vpath cap_copy.cc $(BASE_DIR)/src/platform
vpath %.cc        $(REP_DIR)/src/base
vpath %.cc        $(BASE_DIR)/src/base
