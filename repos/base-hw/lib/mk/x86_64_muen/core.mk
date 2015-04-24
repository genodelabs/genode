#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/x86_64_muen
INC_DIR += $(REP_DIR)/src/core/include/spec/x86_64
INC_DIR += $(REP_DIR)/src/core/include/spec/x86

# add assembly sources
SRC_S += spec/x86_64/mode_transition.s
SRC_S += spec/x86_64/kernel/crt0.s
SRC_S += spec/x86_64_muen/kernel/crt0_translation_table.s
SRC_S += spec/x86_64/crt0.s

# add C++ sources
SRC_CC += spec/x86_64_muen/platform_support.cc
SRC_CC += spec/x86_64_muen/sinfo.cc
SRC_CC += spec/x86_64_muen/kernel/thread.cc
SRC_CC += spec/x86_64_muen/kernel/cpu.cc
SRC_CC += spec/x86_64/kernel/thread_base.cc
SRC_CC += spec/x86_64/idt.cc
SRC_CC += spec/x86_64/tss.cc
SRC_CC += spec/x86/platform_support.cc
SRC_CC += spec/x86/kernel/pd.cc
SRC_CC += spec/x86/cpu.cc
SRC_CC += x86/io_port_session_component.cc
SRC_CC += x86/platform_services.cc
SRC_CC += kernel/vm_thread.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/core.inc
