#
# \brief  Build-system configurations for InSign Arndale 5
# \author Martin Stein
# \date   2013-01-09
#

# denote specs that are fullfilled by this spec
SPECS += exynos5

# add repository relative paths
REP_INC_DIR += include/spec/arndale

# include implied specs
include $(BASE_DIR)/mk/spec/exynos5.mk

