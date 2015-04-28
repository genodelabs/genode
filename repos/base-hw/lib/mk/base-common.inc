#
# \brief  Portions of base library shared by core and non-core processes
# \author Norman Feske
# \author Martin Stein
# \date   2013-02-14
#

# add library dependencies
LIBS += cxx

# add C++ sources
SRC_CC += ipc/ipc.cc
SRC_CC += ipc/ipc_marshal_cap.cc
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
SRC_CC += signal/signal.cc signal/common.cc signal/platform.cc
SRC_CC += server/server.cc
SRC_CC += server/common.cc
SRC_CC += thread/thread.cc
SRC_CC += thread/myself.cc
SRC_CC += thread/bootstrap.cc
SRC_CC += thread/trace.cc
SRC_CC += thread/context_allocator.cc
SRC_CC += kernel/interface.cc

# add include paths
INC_DIR +=  $(REP_DIR)/src/base/lock
INC_DIR += $(BASE_DIR)/src/base/lock
INC_DIR += $(BASE_DIR)/src/base/thread

# declare source locations
vpath %  $(REP_DIR)/src/base
vpath % $(BASE_DIR)/src/base
