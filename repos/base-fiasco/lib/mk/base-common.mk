#
# \brief  Portions of base library shared by core and non-core processes
# \author Norman Feske
# \date   2013-02-14
#

include $(BASE_DIR)/lib/mk/base-common.inc

LIBS += startup

SRC_CC += capability.cc capability_raw.cc
SRC_CC += signal_submit.cc
SRC_CC += rpc_dispatch_loop.cc
SRC_CC += thread.cc thread_bootstrap.cc thread_myself.cc
