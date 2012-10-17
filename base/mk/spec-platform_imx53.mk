#
# \brief  Build-system configurations specific to i.MX53
# \author Stefan Kalkowski
# \date   2012-10-15
#

# denote wich specs are also fullfilled by this spec
SPECS += cortex_a8 imx53 imx

# add repository relative include paths
REP_INC_DIR += include/platform/imx53

# include implied specs
include $(call select_from_repositories,mk/spec-cortex_a8.mk)
