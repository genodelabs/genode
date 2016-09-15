#
# \brief  Offer build configurations that are specific to base-hw and Pandaboard A2
# \author Martin Stein
# \date   2011-12-20
#

# denote wich specs are also fullfilled by this spec
SPECS += hw panda

# configure multiprocessor mode
NR_OF_CPUS = 2

# include implied specs
include $(call select_from_repositories,mk/spec/hw.mk)
include $(call select_from_repositories,mk/spec/panda.mk)
