#
# OKLinux support library
#
SRC_CC   = genode_block.cc \
           genode_config.cc \
           genode_exit.cc \
           genode_framebuffer.cc \
           genode_input.cc \
           genode_lock.cc \
           genode_memory.cc \
           genode_net.cc \
           genode_threads.cc \
           genode_open.cc \
           genode_printf.cc \
           genode_sleep.cc
SRC_CC  += iguana_eas.cc \
           iguana_hardware.cc \
           iguana_memsection.cc \
           iguana_pd.cc \
           iguana_thread.cc \
           iguana_tls.cc
INC_DIR += $(REP_DIR)/include/oklx_lib
INC_DIR += $(REP_DIR)/src/lib/oklx/include
LIBS     = base

# do not produce position-independent code
CC_OPT_PIC =

vpath %.cc $(REP_DIR)/src/lib/oklx/genode
vpath %.cc $(REP_DIR)/src/lib/oklx/iguana

