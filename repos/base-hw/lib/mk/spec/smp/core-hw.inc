#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \date   2016-01-04
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/smp

# add C++ sources
SRC_CC += spec/smp/kernel/kernel.cc
SRC_CC += spec/smp/kernel/cpu.cc
