#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/arm

# add C++ sources
SRC_CC += spec/arm/kernel/thread.cc
SRC_CC += spec/arm/kernel/cpu.cc
SRC_CC += spec/arm/kernel/pd.cc
SRC_CC += spec/arm/platform_support.cc

# add assembly sources
SRC_S += spec/arm/kernel/crt0.s
SRC_S += spec/arm/crt0.s

# include less specific configuration
include $(REP_DIR)/lib/mk/core.inc
