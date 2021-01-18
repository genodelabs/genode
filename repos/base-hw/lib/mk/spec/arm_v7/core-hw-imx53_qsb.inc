#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
REP_INC_DIR += src/core/board/imx53_qsb

SRC_CC += spec/arm/imx_epit.cc
SRC_CC += spec/arm/imx_tzic.cc

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/cortex_a8/core-hw.inc)

