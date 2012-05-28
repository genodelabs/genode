#
# \brief  Build-system configurations specifically for the PandaBoard A2
# \author Martin Stein
# \date   2011-12-20
#

# denote wich specs are also fullfilled by this spec
SPECS += cortex_a9 tl16c750 omap44xx

# add repository relative include paths
REP_INC_DIR += include/platform/panda_a2

# include implied specs
include $(call select_from_repositories,mk/spec-cortex_a9.mk)
include $(call select_from_repositories,mk/spec-tl16c750.mk)

