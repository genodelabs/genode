REP_INC_DIR += src/core/spec/arm_v8

LIBS += syscall-hw

# add C++ sources
SRC_CC += kernel/cpu_mp.cc
SRC_CC += spec/64bit/memory_map.cc
SRC_CC += spec/arm/generic_timer.cc
SRC_CC += spec/arm/kernel/lock.cc
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
vpath spec/64bit/memory_map.cc $(call select_from_repositories,src/lib/hw)

# include less specific configuration
include $(call select_from_repositories,lib/mk/core-hw.inc)
