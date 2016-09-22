#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/x86_64

# add assembly sources
SRC_S += spec/x86_64/mode_transition.s
SRC_S += spec/x86_64/kernel/crt0.s
SRC_S += spec/x86_64/crt0.s

# add C++ sources
SRC_CC += kernel/kernel.cc
SRC_CC += spec/x86/io_port_session_component.cc
SRC_CC += spec/x86/io_port_session_support.cc
SRC_CC += spec/x86_64/bios_data_area.cc
SRC_CC += spec/x86_64/cpu.cc
SRC_CC += spec/x86_64/fpu.cc
SRC_CC += spec/x86_64/idt.cc
SRC_CC += spec/x86_64/kernel/cpu.cc
SRC_CC += spec/x86_64/kernel/cpu_context.cc
SRC_CC += spec/x86_64/kernel/pd.cc
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/platform_support_common.cc
SRC_CC += spec/x86_64/tss.cc

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/core.inc
