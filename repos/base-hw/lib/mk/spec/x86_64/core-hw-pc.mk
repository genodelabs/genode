#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
REP_INC_DIR += src/core/board/pc
REP_INC_DIR += src/core/spec/x86_64
REP_INC_DIR += src/core/spec/x86_64/virtualization

LIBS += syscall-hw

# add assembly sources
SRC_S += spec/x86_64/crt0.s
SRC_S += spec/x86_64/exception_vector.s

# add C++ sources
SRC_CC += kernel/cpu_mp.cc
SRC_CC += kernel/vm_thread_on.cc
SRC_CC += spec/x86_64/virtualization/kernel/vm.cc
SRC_CC += spec/x86_64/virtualization/kernel/svm.cc
SRC_CC += spec/x86_64/virtualization/kernel/vmx.cc
SRC_CC += kernel/lock.cc
SRC_CC += spec/x86_64/pic.cc
SRC_CC += spec/x86_64/timer.cc
SRC_CC += spec/x86_64/kernel/thread_exception.cc
SRC_CC += spec/x86_64/platform_support.cc
SRC_CC += spec/x86_64/virtualization/platform_services.cc

SRC_CC += spec/x86/io_port_session_component.cc
SRC_CC += spec/x86/io_port_session_support.cc
SRC_CC += spec/x86_64/bios_data_area.cc
SRC_CC += spec/x86_64/cpu.cc
SRC_CC += spec/x86_64/kernel/cpu.cc
SRC_CC += spec/x86_64/kernel/pd.cc
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/platform_support_common.cc

PD_SESSION_SUPPORT_CC_PATH := \
   $(call select_from_repositories,src/core/spec/x86_64/pd_session_support.cc)

vpath pd_session_support.cc    $(dir $(PD_SESSION_SUPPORT_CC_PATH))

ARCH_WIDTH_PATH := spec/64bit

# include less specific configuration
include $(call select_from_repositories,lib/mk/core-hw.inc)
