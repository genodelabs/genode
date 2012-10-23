#
# \brief   Parts of core that depend on the PandaBoard A2
# \author  Martin Stein
# \date    2012-04-27
#

# declare location of core files that are board specific
BOARD_DIR = $(REP_DIR)/src/core/panda_a2

# include generic parts of core support
include $(REP_DIR)/lib/mk/arm_v7/core_support.inc
include $(REP_DIR)/lib/mk/core_support.inc

