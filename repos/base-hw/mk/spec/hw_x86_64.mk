#
# \brief  Offer build configurations that are specific to base-hw and x86_64
# \author Martin Stein
# \date   2011-12-20
#

# denote wich specs are also fullfilled by this spec
SPECS += hw x86_64
SPECS += pci ps2 vesa framebuffer

# configure multiprocessor mode
NR_OF_CPUS = 1

# include implied specs
include $(call select_from_repositories,mk/spec/hw.mk)
include $(call select_from_repositories,mk/spec/x86_64.mk)
