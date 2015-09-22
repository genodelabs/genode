#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2015-02-09
#

# add C++ sources
SRC_CC += spec/exynos5/board.cc
SRC_CC += spec/arm_gic/pic.cc
SRC_CC += platform_services.cc
SRC_CC += kernel/vm_thread.cc
SRC_CC += spec/arm/kernel/cpu_context.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/exynos5/core.inc
