#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \author Stefan Kalkowski
# \date   2012-10-04
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/arm_v6

# add C++ sources
SRC_CC += spec/arm_v6/cpu.cc
SRC_CC += spec/arm_v6/perf_counter.cc
SRC_CC += spec/arm/kernel/cpu_context.cc
SRC_CC += spec/arm/kernel/cpu.cc
SRC_CC += spec/arm/kernel/thread_update_pd.cc
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += kernel/kernel.cc

# add assembly sources
SRC_S += spec/arm_v6/mode_transition.s

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/spec/arm/core-hw.inc
