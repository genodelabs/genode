#
# \brief  Offer build configurations that are specific to base-hw and Arndale
# \author Martin Stein
# \date   2013-01-09
#

# denote wich specs are also fullfilled by this spec
SPECS += hw platform_arndale

# configure multiprocessor mode
NR_OF_CPUS = 2

# add repository relative paths
REP_INC_DIR += include/exynos5_uart

# set address where to link the text segment at
LD_TEXT_ADDR ?= 0x80000000

# include implied specs
include $(call select_from_repositories,mk/spec-hw.mk)
include $(call select_from_repositories,mk/spec-platform_arndale.mk)
