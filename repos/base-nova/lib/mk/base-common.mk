#
# \brief  Portions of base library shared by core and non-core processes
# \author Norman Feske
# \date   2013-02-14
#

include $(BASE_DIR)/lib/mk/base-common.inc

LIBS += startup

SRC_CC += signal_submit.cc
SRC_CC += thread.cc thread_myself.cc
SRC_CC += stack.cc
SRC_CC += cap_map.cc
SRC_CC += capability.cc
