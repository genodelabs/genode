#
# \brief  Build-system configurations for Odrod-x2
# \author Alexy Gallardo Segura <alexy@uclv.cu>
# \author Humberto López León <humberto@uclv.cu>
# \author Reinier Millo Sánchez <rmillo@uclv.cu>
# \date   2015-04-28
#

# denote specs that are fullfilled by this spec
SPECS +=  exynos4 cortex_a9

# add repository relative paths
REP_INC_DIR += include/platform/odroid_x2
REP_INC_DIR += include/platform/exynos4

# include implied specs
include $(call select_from_repositories,mk/spec-cortex_a9.mk)
