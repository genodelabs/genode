#
# \brief  Portions of base library shared by core and non-core processes
# \author Norman Feske
# \date   2013-02-14
#

include $(BASE_DIR)/lib/mk/base-common.inc

LIBS += syscall-foc startup-foc

SRC_CC += spin_lock.cc cap_map.cc
SRC_CC += signal_submit.cc
SRC_CC += rpc_dispatch_loop.cc
SRC_CC += thread.cc thread_bootstrap.cc thread_myself.cc utcb.cc
SRC_CC += capability.cc
SRC_CC += signal_source_client.cc
