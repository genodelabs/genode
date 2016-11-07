#
# \brief  Build config for Genodes core process
# \author Johannes Schlatow
# \date   2014-12-15
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/zynq

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/zynq/platform_support.cc

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/spec/cortex_a9/core.inc
