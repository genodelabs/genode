#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/arm_v6

# add C++ sources
SRC_CC += cpu.cc
SRC_CC += spec/arm_v6/cpu.cc

# add assembly sources
SRC_S += spec/arm_v6/mode_transition.s

# include less specific configuration
include $(REP_DIR)/lib/mk/arm/core.inc
