#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
REP_INC_DIR += src/core/board/pbxa9

# add C++ sources
SRC_CC += platform_services.cc

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/arm_v7/core-hw-cortex_a9.inc)
