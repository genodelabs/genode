#
# \brief  Build-system configurations for ARM Cortex A15
# \author Martin Stein
# \date   2013-01-09
#

# denote wich specs are also fullfilled by this spec
SPECS += arm_v7a

# add repository relative include paths
REP_INC_DIR += include/cortex_a15

# include implied specs
include $(call select_from_repositories,mk/spec-arm_v7a.mk)

