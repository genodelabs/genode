#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2015-06-02
#

# add assembly sources
SRC_S += spec/x86_64/kernel/crt0_translation_table.s

# add C++ sources
SRC_CC += spec/x86/pic.cc
SRC_CC += spec/x86/kernel/cpu_exception.cc
SRC_CC += spec/x86/kernel/thread_exception.cc
SRC_CC += spec/x86_64/platform_support.cc

# core.inc files use BASE_HW_DIR in order to allow
# including these files from other repositories
BASE_HW_DIR := $(REP_DIR)

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/x86_64/core.inc
