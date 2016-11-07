#
# \brief  Portions of base library shared by core and non-core processes
# \author Norman Feske
# \date   2013-02-14
#

include $(BASE_DIR)/lib/mk/base-common.inc

LIBS += startup syscall

SRC_CC += signal_submit.cc
SRC_CC += rpc_dispatch_loop.cc
SRC_CC += thread.cc thread_myself.cc thread_bootstrap.cc
SRC_CC += capability.cc capability_raw.cc
