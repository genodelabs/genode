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
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/idt.cc
SRC_CC += spec/x86_64/tss.cc
SRC_CC += spec/x86_64/kernel/cpu_context.cc

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/spec/x86/core.inc
