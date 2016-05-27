#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/panda
INC_DIR += $(REP_DIR)/src/core/include/spec/tl16c750

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/panda/platform_support.cc

# core.inc files use BASE_HW_DIR in order to allow
# including these files from other repositories
BASE_HW_DIR := $(REP_DIR)

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/cortex_a9/core.inc
