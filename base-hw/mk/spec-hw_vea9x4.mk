#
# \brief  Offer build configurations that are specific to base-hw and VEA9X4
# \author Martin Stein
# \date   2011-12-20
#

# denote wich specs are also fullfilled by this spec
SPECS += hw platform_vea9x4

# set address where to link text segment at
LD_TEXT_ADDR ?= 0x01000000

# include implied specs
include $(call select_from_repositories,mk/spec-hw.mk)
include $(call select_from_repositories,mk/spec-platform_vea9x4.mk)
