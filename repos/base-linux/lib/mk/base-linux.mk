#
# \brief  Base lib parts that are not used by hybrid applications
# \author Sebastian Sumpf
# \date   2014-02-21
#

include $(REP_DIR)/lib/mk/base-linux.inc

LIBS   += startup-linux base-linux-common cxx timeout
SRC_CC += thread.cc thread_myself.cc thread_linux.cc
SRC_CC += capability_space.cc capability_raw.cc
SRC_CC += attach_stack_area.cc
SRC_CC += signal_transmitter.cc signal.cc
