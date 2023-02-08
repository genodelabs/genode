#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2014-09-02
#

# add C++ sources
SRC_CC += kernel/cpu_up.cc
SRC_CC += kernel/lock.cc

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/arm_v7/core-hw.inc)
