#
# \brief  Build config for Genodes core process
# \author Norman Feske
# \date   2013-04-05
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/rpi
INC_DIR += $(REP_DIR)/src/core/include/spec/pl011

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/rpi/platform_support.cc

# core.inc files use BASE_HW_DIR in order to allow
# including these files from other repositories
BASE_HW_DIR := $(REP_DIR)

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/arm_v6/core.inc
