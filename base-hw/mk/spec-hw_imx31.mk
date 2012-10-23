#
# \brief  Build configurations for 'base-hw' on Freescale i.MX31
# \author Martin Stein
# \date   2011-12-20
#

# denote wich specs are also fullfilled by this spec
SPECS += hw platform_imx31

# set address where to link the text segment at
LD_TEXT_ADDR ?= 0x82000000

# include implied specs
include $(call select_from_repositories,mk/spec-hw.mk)
include $(call select_from_repositories,mk/spec-platform_imx31.mk)
