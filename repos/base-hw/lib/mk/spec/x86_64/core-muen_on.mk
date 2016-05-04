#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2015-06-02
#

REQUIRES = muen

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/x86_64/muen

# add assembly sources
SRC_S += spec/x86_64/muen/kernel/crt0_translation_table.s

# add C++ sources
SRC_CC += spec/x86_64/muen/kernel/cpu_exception.cc
SRC_CC += spec/x86_64/muen/kernel/thread_exception.cc
SRC_CC += spec/x86_64/muen/platform_support.cc
SRC_CC += spec/x86_64/muen/sinfo.cc

# core.inc files use BASE_HW_DIR in order to allow
# including these files from other repositories
BASE_HW_DIR := $(REP_DIR)

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/x86_64/core.inc
