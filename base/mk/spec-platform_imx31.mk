#
# \brief  Build-system configurations for Freescale i.MX31
# \author Martin Stein
# \date   2012-09-26
#

# denote wich specs are also fullfilled by this spec
SPECS += arm_v6

# add repository relative include paths
REP_INC_DIR += include/platform/imx31

# include implied specs
include $(call select_from_repositories,mk/spec-arm_v6.mk)

