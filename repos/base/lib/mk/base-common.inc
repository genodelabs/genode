#
# \brief  Portions of base library shared by core and non-core processes
# \author Norman Feske
# \date   2013-02-14
#

SRC_CC += avl_tree.cc
SRC_CC += slab.cc
SRC_CC += allocator_avl.cc
SRC_CC += heap.cc sliced_heap.cc
SRC_CC += registry.cc
SRC_CC += output.cc
SRC_CC += child.cc
SRC_CC += child_process.cc
SRC_CC += session_state.cc
SRC_CC += elf_binary.cc
SRC_CC += ipc.cc
SRC_CC += lock.cc
SRC_CC += mutex.cc
SRC_CC += log.cc
SRC_CC += raw_output.cc
SRC_CC += rpc_entrypoint.cc
SRC_CC += signal_common.cc
SRC_CC += sleep.cc
SRC_CC += region_map_client.cc
SRC_CC += rm_session_client.cc
SRC_CC += stack_allocator.cc
SRC_CC += trace.cc
SRC_CC += trace_buffer.cc
SRC_CC += env_session_id_space.cc
SRC_CC += stack_protector.cc
SRC_CC += xml_generator.cc

INC_DIR += $(REP_DIR)/src/include $(BASE_DIR)/src/include

vpath %.cc  $(REP_DIR)/src/lib/base
vpath %.cc $(BASE_DIR)/src/lib/base
