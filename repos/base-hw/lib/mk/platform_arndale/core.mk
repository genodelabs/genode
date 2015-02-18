#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2015-02-09
#

# add C++ sources
SRC_CC += spec/arndale/board.cc
SRC_CC += spec/arndale/pic.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/exynos5/core.inc
