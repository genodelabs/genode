#
# \brief  Build-system configurations for Exynos5 SoCs
# \author Stefan Kalkowski
# \date   2015-09-04
#

# denote specs that are fullfilled by this spec
SPECS += exynos cortex_a15 framebuffer usb

# add repository relative paths
REP_INC_DIR += include/spec/exynos5
REP_INC_DIR += include/spec/exynos

# include implied specs
include $(call select_from_repositories,mk/spec/cortex_a15.mk)
