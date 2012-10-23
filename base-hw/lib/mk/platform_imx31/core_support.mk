#
# \brief   Parts of core that depend on i.MX31
# \author  Norman Feske
# \author  Martin Stein
# \date    2012-08-30
#

# declare location of core files that are board specific
BOARD_DIR = $(REP_DIR)/src/core/imx31

# add includes to search path
INC_DIR += $(REP_DIR)/src/core/include/imx31

# include less specific parts
include $(REP_DIR)/lib/mk/arm_v6/core_support.inc
include $(REP_DIR)/lib/mk/core_support.inc

