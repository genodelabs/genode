#
# \brief  Portions of base library shared by core and non-core processes
# \author Norman Feske
# \date   2013-02-14
#

LIBS += cxx startup syscall

SRC_CC += cap_copy.cc
SRC_CC += ipc/ipc.cc ipc/ipc_marshal_cap.cc
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
SRC_CC += thread/thread.cc thread/trace.cc thread/thread_bootstrap.cc
SRC_CC += thread/myself.cc
SRC_CC += thread/stack_allocator.cc
SRC_CC += sleep.cc
SRC_CC += rm_session_client.cc
SRC_CC += entrypoint/entrypoint.cc
SRC_CC += component/component.cc

# suppress warning caused by Pistachio's 'l4/message.h'
CC_WARN += -Wno-array-bounds

INC_DIR += $(REP_DIR)/src/include $(BASE_DIR)/src/include

vpath cap_copy.cc $(BASE_DIR)/src/lib/startup
vpath %.cc        $(REP_DIR)/src/base
vpath %.cc        $(BASE_DIR)/src/base
