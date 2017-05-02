#
# \brief  Build-system configurations for Odrod-x2
# \author Alexy Gallardo Segura <alexy@uclv.cu>
# \author Humberto López León <humberto@uclv.cu>
# \author Reinier Millo Sánchez <rmillo@uclv.cu>
# \date   2015-07-08
#

# denote specs that are fullfilled by this spec
SPECS += exynos4 cortex_a9 usb framebuffer gpio

# add repository relative paths
REP_INC_DIR += include/spec/odroid_x2
REP_INC_DIR += include/spec/exynos4

# include implied specs
include $(BASE_DIR)/mk/spec/cortex_a9.mk
