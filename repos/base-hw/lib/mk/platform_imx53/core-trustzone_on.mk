#
# \brief  Build config for parts of core that depend on Trustzone status
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/imx53/trustzone
INC_DIR += $(REP_DIR)/src/core/include/spec/imx53
INC_DIR += $(REP_DIR)/src/core/include/spec/cortex_a8

# add C++ sources
SRC_CC += spec/imx53/trustzone/platform_support.cc
SRC_CC += spec/imx53/trustzone/platform_services.cc
SRC_CC += spec/imx53/trustzone/pic.cc
SRC_CC += vm_session_component.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/core-trustzone.inc
