#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/x86_64

# add assembly sources
SRC_S += spec/x86_64/mode_transition.s
SRC_S += spec/x86_64/kernel/crt0.s
SRC_S += spec/x86_64/crt0.s

# add C++ sources
SRC_CC += spec/x86_64/kernel/thread_base.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/x86/core.inc
