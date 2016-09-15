#
# \brief  Build configurations for 'base-hw' on USB Armory
# \author Martin Stein
# \date   2015-02-24
#

# denote wich specs are also fullfilled by this spec
SPECS += hw usb_armory imx53 trustzone

# configure multiprocessor mode
NR_OF_CPUS = 1

# add repository relative include paths
REP_INC_DIR += include/spec/usb_armory

# include implied specs
include $(call select_from_repositories,mk/spec/hw.mk)
include $(call select_from_repositories,mk/spec/imx53.mk)
