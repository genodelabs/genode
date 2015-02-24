#
# \brief  Build configurations for 'base-hw' on USB Armory
# \author Martin Stein
# \date   2015-02-24
#

# denote wich specs are also fullfilled by this spec
SPECS += hw platform_imx53 platform_usb_armory epit trustzone

# configure multiprocessor mode
NR_OF_CPUS = 1

# set address where to link the text segment at
LD_TEXT_ADDR ?= 0x72000000

# add repository relative include paths
REP_INC_DIR += include/platform/usb_armory

# include implied specs
include $(call select_from_repositories,mk/spec-hw.mk)
include $(call select_from_repositories,mk/spec-platform_imx53.mk)
