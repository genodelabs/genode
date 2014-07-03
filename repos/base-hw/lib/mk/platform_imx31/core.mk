#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/imx31

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += platform_support.cc

# declare source paths
vpath platform_services.cc $(BASE_DIR)/src/core
vpath platform_support.cc   $(REP_DIR)/src/core/imx31

# include less specific library parts
include $(REP_DIR)/lib/mk/arm_v6/core.inc
