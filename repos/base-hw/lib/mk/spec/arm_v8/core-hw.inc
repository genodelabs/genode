#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2019-05-21
#

# add libraries
LIBS += syscall-hw

# add C++ sources
SRC_CC += kernel/cpu_mp.cc
SRC_CC += spec/arm/generic_timer.cc
SRC_CC += spec/arm/kernel/lock.cc
SRC_CC += spec/arm/kernel/thread_caches.cc
SRC_CC += spec/arm/platform_support.cc
SRC_CC += spec/arm_v8/cpu.cc
SRC_CC += spec/arm_v8/kernel/cpu.cc
SRC_CC += spec/arm_v8/kernel/thread.cc

#add assembly sources
SRC_S += spec/arm_v8/exception_vector.s
SRC_S += spec/arm_v8/crt0.s

PD_SESSION_SUPPORT_CC_PATH := \
   $(call select_from_repositories,src/core/spec/arm_v8/pd_session_support.cc)

vpath pd_session_support.cc    $(dir $(PD_SESSION_SUPPORT_CC_PATH))

ARCH_WIDTH_PATH := spec/64bit

# include less specific configuration
include $(call select_from_repositories,lib/mk/core-hw.inc)
