#
# \brief  Portions of base library shared by core and non-core components
# \author Norman Feske
# \author Martin Stein
# \date   2013-02-14
#

LIBS += cxx

SRC_CC += ipc/ipc.cc
SRC_CC += avl_tree/avl_tree.cc
SRC_CC += allocator/slab.cc
SRC_CC += allocator/allocator_avl.cc
SRC_CC += heap/heap.cc
SRC_CC += heap/sliced_heap.cc
SRC_CC += child/child.cc
SRC_CC += process/process.cc
SRC_CC += elf/elf_binary.cc
SRC_CC += console/console.cc
SRC_CC += lock/lock.cc
SRC_CC += server/server.cc
SRC_CC += server/common.cc
SRC_CC += signal/common.cc signal/signal.cc
SRC_CC += thread/thread.cc
SRC_CC += thread/myself.cc
SRC_CC += thread/bootstrap.cc
SRC_CC += thread/trace.cc
SRC_CC += thread/context_allocator.cc
SRC_CC += kernel/interface.cc
SRC_CC += sleep.cc
SRC_CC += rm_session_client.cc

INC_DIR += $(REP_DIR)/src/include $(BASE_DIR)/src/include

vpath %  $(REP_DIR)/src/base
vpath % $(BASE_DIR)/src/base
