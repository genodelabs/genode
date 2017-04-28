#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

REQUIRES = muen

LIBS += sinfo-muen

# add include paths
INC_DIR += $(REP_DIR)/src/core/spec/x86_64/muen
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/x86_64

# add assembly sources
SRC_S += spec/x86_64/mode_transition.s
SRC_S += spec/x86_64/crt0.s

# add C++ sources
SRC_CC += spec/x86_64/muen/kernel/cpu_exception.cc
SRC_CC += spec/x86_64/muen/kernel/thread_exception.cc
SRC_CC += spec/x86_64/muen/platform_support.cc
SRC_CC += spec/x86_64/muen/kernel/vm.cc
SRC_CC += spec/x86_64/muen/platform_services.cc
SRC_CC += spec/x86_64/muen/sinfo_instance.cc
SRC_CC += spec/x86_64/muen/timer.cc
SRC_CC += kernel/vm_thread_on.cc

SRC_CC += kernel/kernel.cc
SRC_CC += spec/x86/io_port_session_component.cc
SRC_CC += spec/x86/io_port_session_support.cc
SRC_CC += spec/x86_64/bios_data_area.cc
SRC_CC += spec/x86_64/cpu.cc
SRC_CC += spec/x86_64/fpu.cc
SRC_CC += spec/x86_64/kernel/cpu.cc
SRC_CC += spec/x86_64/kernel/cpu_context.cc
SRC_CC += spec/x86_64/kernel/pd.cc
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/platform_support_common.cc

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/core-hw.inc
