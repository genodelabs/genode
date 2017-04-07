#
# \brief  Build-system configurations for Wandboard Quad
# \author Stefan Kalkowski
# \date   2017-01-02
#

# denote wich specs are also fullfilled by this spec
SPECS += imx6

# add repository relative include paths
REP_INC_DIR += include/spec/wand_quad

# include implied specs
include $(BASE_DIR)/mk/spec/imx6.mk
