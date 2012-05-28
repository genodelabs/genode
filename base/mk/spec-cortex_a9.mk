#
# \brief  Build-system configurations specifically for the ARM Cortex A9
# \author Martin Stein
# \date   2011-12-20
#

# denote wich specs are also fullfilled by this spec
SPECS += arm_v7a pl390

# add repository relative include paths
REP_INC_DIR += include/cortex_a9

# include implied specs
include $(call select_from_repositories,mk/spec-arm_v7a.mk)
include $(call select_from_repositories,mk/spec-pl390.mk)

