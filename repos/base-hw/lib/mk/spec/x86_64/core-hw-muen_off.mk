#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2015-06-02
#

# add C++ sources
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += spec/x86_64/pic.cc
SRC_CC += spec/x86_64/timer.cc
SRC_CC += spec/x86_64/kernel/cpu_exception.cc
SRC_CC += spec/x86_64/kernel/thread_exception.cc
SRC_CC += spec/x86_64/platform_support.cc
SRC_CC += spec/x86/platform_services.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/x86_64/core-hw.inc
