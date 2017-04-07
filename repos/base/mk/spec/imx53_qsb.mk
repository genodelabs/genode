#
# \brief  Build-system configurations specific to i.MX53 Quickstart Board
# \author Stefan Kalkowski
# \date   2017-01-02
#

# denote wich specs are also fullfilled by this spec
SPECS += imx53

# add repository relative include paths
REP_INC_DIR += include/spec/imx53_qsb

# include implied specs
include $(BASE_DIR)/mk/spec/imx53.mk
