#
# \brief  Build-system configurations for Odroid XU
# \author Stefan Kalkowski
# \date   2013-11-25
#

# denote specs that are fullfilled by this spec
SPECS += exynos5

# add repository relative paths
REP_INC_DIR += include/spec/odroid_xu

# include implied specs
include $(BASE_DIR)/mk/spec/exynos5.mk

