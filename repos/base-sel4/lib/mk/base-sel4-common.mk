#
# \brief  Portions of base library shared by core and non-core processes
# \author Norman Feske
# \date   2013-02-14
#

include $(BASE_DIR)/lib/mk/base-common.inc

LIBS += startup-sel4 syscall-sel4

SRC_CC += rpc_dispatch_loop.cc
SRC_CC += thread.cc thread_myself.cc thread_bootstrap.cc
SRC_CC += capability.cc capability_raw.cc
