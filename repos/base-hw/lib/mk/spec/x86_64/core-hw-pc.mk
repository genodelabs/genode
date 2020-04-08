#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/spec/x86_64

# add assembly sources
SRC_S += spec/x86_64/crt0.s
SRC_S += spec/x86_64/exception_vector.s

# add C++ sources
SRC_CC += kernel/cpu_mp.cc
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += kernel/lock.cc
SRC_CC += spec/x86_64/pic.cc
SRC_CC += spec/x86_64/pit.cc
SRC_CC += spec/x86_64/kernel/thread_exception.cc
SRC_CC += spec/x86_64/platform_support.cc
SRC_CC += spec/x86/platform_services.cc

SRC_CC += spec/x86/io_port_session_component.cc
SRC_CC += spec/x86/io_port_session_support.cc
SRC_CC += spec/x86_64/bios_data_area.cc
SRC_CC += spec/x86_64/cpu.cc
SRC_CC += spec/x86_64/kernel/cpu.cc
SRC_CC += spec/x86_64/kernel/pd.cc
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/platform_support_common.cc

SRC_CC += spec/64bit/memory_map.cc

vpath spec/64bit/memory_map.cc $(REP_DIR)/src/lib/hw

NR_OF_CPUS = 32

# include less specific configuration
include $(REP_DIR)/lib/mk/core-hw.inc
