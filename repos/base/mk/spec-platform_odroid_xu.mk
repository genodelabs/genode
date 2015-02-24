#
# \brief  Build-system configurations for Odroid XU
# \author Stefan Kalkowski
# \date   2013-11-25
#

# denote specs that are fullfilled by this spec
SPECS += exynos5 cortex_a15

# add repository relative paths
REP_INC_DIR += include/platform/odroid_xu
REP_INC_DIR += include/platform/exynos5

# include implied specs
include $(call select_from_repositories,mk/spec-cortex_a15.mk)

