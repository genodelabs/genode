#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

# add C++ sources
SRC_CC += spec/arm/cortex_a15_cpu.cc
SRC_CC += kernel/cpu_mp.cc
SRC_CC += spec/arm/kernel/mutex.cc

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/arm_v7/core-hw.inc)
