#
# \brief  Build-system configurations for Exynos5 SoCs
# \author Stefan Kalkowski
# \date   2015-09-04
#

# denote specs that are fullfilled by this spec
SPECS += cortex_a15 framebuffer usb

# add repository relative paths
REP_INC_DIR += include/spec/exynos5

# include implied specs
include $(BASE_DIR)/mk/spec/cortex_a15.mk
