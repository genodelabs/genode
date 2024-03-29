#
# \brief  Portions of base library shared by core and non-core components
# \author Norman Feske
# \author Martin Stein
# \date   2013-02-14
#

include $(BASE_DIR)/lib/mk/base-common.inc

SRC_CC += rpc_dispatch_loop.cc
SRC_CC += thread.cc thread_myself.cc thread_bootstrap.cc
SRC_CC += signal_transmitter.cc

# filter out log.cc from the generic base library
# in core and hw kernel we have to implement it differently
SRC_CC_WITH_LOG_CC := $(SRC_CC)
SRC_CC = $(filter-out log.cc,$(SRC_CC_WITH_LOG_CC))
