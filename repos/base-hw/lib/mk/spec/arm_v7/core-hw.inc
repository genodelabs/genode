#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
REP_INC_DIR += src/core/spec/arm_v7

# add C++ sources
SRC_CC += spec/arm_v7/perf_counter.cc

SRC_S  += spec/arm/vfpv3-d32.s

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/arm/core-hw.inc)
