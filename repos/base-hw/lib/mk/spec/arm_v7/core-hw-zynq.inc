#
# \brief  Build config for Genodes core process
# \author Johannes Schlatow
# \date   2014-12-15
#

# add C++ sources
SRC_CC += platform_services.cc

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/cortex_a9/core-hw.inc)
