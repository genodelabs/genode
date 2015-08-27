#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2015-06-02
#

# add assembly sources
SRC_S += spec/x86_64/kernel/crt0_translation_table.s

# add C++ sources
SRC_CC += spec/x86/kernel/thread.cc
SRC_CC += spec/x86/kernel/cpu.cc
SRC_CC += spec/x86/pic.cc
SRC_CC += spec/x86_64/platform_support.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/x86_64/core.inc
