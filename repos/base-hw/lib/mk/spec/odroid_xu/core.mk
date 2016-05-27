#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2015-02-09
#

# add C++ sources
SRC_CC += spec/cortex_a15/cpu_init.cc
SRC_CC += spec/arm/kernel/cpu_context.cc
SRC_CC += spec/arm_gic/pic.cc
SRC_CC += kernel/vm_thread.cc
SRC_CC += platform_services.cc

# core.inc files use BASE_HW_DIR in order to allow
# including these files from other repositories
BASE_HW_DIR := $(REP_DIR)

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/exynos5/core.inc
