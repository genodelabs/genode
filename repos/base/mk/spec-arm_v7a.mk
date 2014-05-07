#
# \brief  Build-system configurations for ARMv7a
# \author Martin Stein
# \date   2012-09-26
#

# denote wich specs are also fullfilled by this spec
SPECS += arm_v7

# configure compiler
CC_MARCH += -march=armv7-a

# add repository relative include paths
REP_INC_DIR += include/arm_v7a

# include implied specs
include $(call select_from_repositories,mk/spec-arm_v7.mk)

