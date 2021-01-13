#
# \brief  Build config for Genodes core process
# \author Johannes Schlatow
# \date   2014-12-15
#

TMP := $(call select_from_repositories,lib/mk/core-hw.inc)
BASE_HW_DIR := $(TMP:%lib/mk/core-hw.inc=%)

# add include paths
INC_DIR += $(BASE_HW_DIR)/src/core/spec/zynq

# add C++ sources
SRC_CC += platform_services.cc

# include less specific configuration
include $(BASE_HW_DIR)/lib/mk/spec/cortex_a9/core-hw.inc
