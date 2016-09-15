#
# \brief  Offer build configurations that are specific to base-hw and Odroid XU
# \author Stefan Kalkowski
# \date   2013-11-25
#

# denote wich specs are also fullfilled by this spec
SPECS += hw odroid_xu

# configure multiprocessor mode
NR_OF_CPUS = 1

# add repository relative paths
REP_INC_DIR += include/exynos5_uart

# include implied specs
include $(call select_from_repositories,mk/spec/hw.mk)
include $(call select_from_repositories,mk/spec/odroid_xu.mk)
