#
# \brief  Build config for parts of core that depend on Trustzone status
# \author Martin Stein
# \date   2015-10-30
#

# add include paths
REP_INC_DIR += src/core/board/usb_armory
REP_INC_DIR += src/core/spec/arm_v7/trustzone

# add C++ sources
SRC_CC += kernel/vm_thread_on.cc
SRC_CC += spec/arm/imx_epit.cc
SRC_CC += spec/arm/imx_tzic.cc
SRC_CC += spec/arm_v7/trustzone/kernel/vm.cc
SRC_CC += spec/arm_v7/trustzone/platform_services.cc
SRC_CC += spec/arm_v7/trustzone/vm_session_component.cc
SRC_CC += vm_session_common.cc
SRC_CC += vm_session_component.cc

# add assembly sources
SRC_S += spec/arm_v7/trustzone/exception_vector.s

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/cortex_a8/core-hw.inc)
