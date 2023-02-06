#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2015-02-09
#

# add include paths
REP_INC_DIR += src/core/board/imx7d_sabre
REP_INC_DIR += src/core/spec/arm/virtualization

# add C++ sources
SRC_CC += kernel/vm_thread_on.cc
SRC_CC += spec/arm/generic_timer.cc
SRC_CC += spec/arm/gicv2.cc
SRC_CC += spec/arm_v7/virtualization/kernel/vm.cc
SRC_CC += spec/arm/virtualization/gicv2.cc
SRC_CC += spec/arm/virtualization/platform_services.cc
SRC_CC += spec/arm/virtualization/vm_session_component.cc
SRC_CC += vm_session_common.cc
SRC_CC += vm_session_component.cc

# add assembly sources
SRC_S += spec/arm_v7/virtualization/exception_vector.s

#
# we need more specific compiler hints for some 'special' assembly code
# override -march=armv7-a because it conflicts with -mcpu=cortex-a7
#
CC_MARCH = -mcpu=cortex-a7 -mfpu=vfpv3 -mfloat-abi=softfp

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/cortex_a15/core-hw.inc)
