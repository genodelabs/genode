#
# \brief  Build-system configurations for InSign Arndale 5
# \author Martin Stein
# \date   2013-01-09
#

# denote specs that are fullfilled by this spec
SPECS += exynos5 cortex_a15 framebuffer usb

# add repository relative paths
REP_INC_DIR += include/platform/arndale
REP_INC_DIR += include/platform/exynos5

# include implied specs
include $(call select_from_repositories,mk/spec-cortex_a15.mk)

