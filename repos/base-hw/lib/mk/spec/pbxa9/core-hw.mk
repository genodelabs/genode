#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/pbxa9
INC_DIR += $(REP_DIR)/src/core/include/spec/pl011

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/pbxa9/platform_support.cc
SRC_CC += spec/pbxa9/board.cc

NR_OF_CPUS = 1

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/cortex_a9/core-hw.inc
