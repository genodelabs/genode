#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/spec/panda

# add C++ sources
SRC_CC += platform_services.cc

NR_OF_CPUS += 2

CC_MARCH = -mcpu=cortex-a9

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/cortex_a9/core-hw.inc
