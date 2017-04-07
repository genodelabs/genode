#
# \brief  Build-system configurations specific to the USB armory
# \author Stefan Kalkowski
# \date   2017-01-02
#

# denote wich specs are also fullfilled by this spec
SPECS += imx53

# add repository relative include paths
REP_INC_DIR += include/spec/usb_armory

# include implied specs
include $(BASE_DIR)/mk/spec/imx53.mk
