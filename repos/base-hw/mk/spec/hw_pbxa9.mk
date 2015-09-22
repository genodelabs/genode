#
# \brief  Offer build configurations that are specific to base-hw and PBXA9
# \author Martin Stein
# \date   2011-12-20
#

# denote wich specs are also fullfilled by this spec
SPECS += hw pbxa9

# configure multiprocessor mode
NR_OF_CPUS = 1

# set address where to link text segment at
LD_TEXT_ADDR ?= 0x70000000

# include implied specs
include $(call select_from_repositories,mk/spec/hw.mk)
include $(call select_from_repositories,mk/spec/pbxa9.mk)
