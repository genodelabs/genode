#
# \brief  Portions of base library shared by core and non-core components
# \author Norman Feske
# \author Martin Stein
# \date   2013-02-14
#

include $(BASE_DIR)/lib/mk/base-common.inc

LIBS += syscall-hw

SRC_CC += rpc_dispatch_loop.cc
SRC_CC += thread.cc thread_myself.cc thread_bootstrap.cc
