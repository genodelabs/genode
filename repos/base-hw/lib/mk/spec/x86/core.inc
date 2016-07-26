#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/x86

# add C++ sources
SRC_CC += kernel/kernel.cc
SRC_CC += spec/x86/kernel/pd.cc
SRC_CC += spec/x86/kernel/cpu.cc
SRC_CC += spec/x86/kernel/thread.cc
SRC_CC += spec/x86/platform_support.cc
SRC_CC += spec/x86/cpu.cc
SRC_CC += spec/x86/fpu.cc
SRC_CC += spec/x86/bios_data_area.cc
SRC_CC += spec/x86/io_port_session_component.cc
SRC_CC += spec/x86/io_port_session_support.cc

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/core.inc
