#
# L4Linux support library
#
SRC_CC  += env.cc \
           dataspace.cc \
           genode_block.cc \
           genode_framebuffer.cc \
           genode_input.cc \
           genode_net.cc \
           genode_terminal.cc \
           genode_string.cc \
           l4_io.cc \
           l4_log.cc \
           l4_re_c_dataspace.cc \
           l4_re_c_debug.cc \
           l4_re_c_mem_alloc.cc \
           l4_re_c_namespace.cc \
           l4_re_c_rm.cc \
           l4_re_c_util_cap.cc \
           l4_re_env.cc \
           l4_util_atomic.cc \
           l4_util_cpu.cc \
           l4_util_kip.cc \
           l4_util_util.cc \
           l4lx_irq.cc \
           l4lx_memory.cc \
           l4lx_task.cc \
           l4lx_thread.cc \
           l4x_pagefault.cc \
           rm.cc \
           startup.cc

INC_DIR += $(REP_DIR)/include \
           $(REP_DIR)/src/lib/l4lx/include \

# for reusing the 'Expanding_rm_session' from the platform env in dataspace.h
INC_DIR += $(BASE_DIR)/src/include

LIBS     = base config

vpath %.cc $(REP_DIR)/src/lib/l4lx
