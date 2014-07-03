#
# \brief  Offer build configurations that are specific to base-hw and VEA9X4
# \author Martin Stein
# \date   2011-12-20
#

# denote wich specs are also fullfilled by this spec
SPECS += hw_vea9x4 trustzone

# adjust link address of a trustzone text segment
LD_TEXT_ADDR = 0x48000000

# include implied specs
include $(call select_from_repositories,mk/spec-hw_vea9x4.mk)
