#
# \brief   Trustzone support for VEA9X4 with TZ enabled
# \author  Stefan Kalkowski
# \date    2012-10-10
#

BOARD_DIR = $(REP_DIR)/src/core/vea9x4

INC_DIR += $(BOARD_DIR)/trustzone

# declare source paths
vpath kernel_support.cc $(BOARD_DIR)/trustzone
vpath trustzone.cc      $(BOARD_DIR)/trustzone

# include generic part of core support
include $(REP_DIR)/lib/mk/arm_v7/core_support.inc
include $(REP_DIR)/lib/mk/core_support.inc

