#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2015-02-09
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/spec/odroid_xu

# add C++ sources
SRC_CC += spec/arm_gic/pic.cc
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += platform_services.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/exynos5/core-hw.inc
