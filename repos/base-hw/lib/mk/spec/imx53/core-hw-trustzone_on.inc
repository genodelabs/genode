#
# \brief  Build config for parts of core that depend on Trustzone status
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/arm_v7/trustzone
INC_DIR += $(REP_DIR)/src/core/include/spec/imx53/trustzone

# add C++ sources
SRC_CC += spec/imx53/trustzone/platform_support.cc
SRC_CC += spec/imx53/trustzone/platform_services.cc
SRC_CC += spec/imx53/trustzone/pic.cc
SRC_CC += kernel/vm_thread_on.cc
SRC_CC += spec/arm_v7/trustzone/kernel/vm.cc
SRC_CC += spec/arm_v7/vm_session_component.cc
SRC_CC += spec/arm_v7/trustzone/vm_session_component.cc

# add assembly sources
SRC_S += spec/arm_v7/trustzone/mode_transition.s

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/imx53/core-hw.inc
