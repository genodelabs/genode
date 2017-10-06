#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/arm

# add C++ sources
SRC_CC += spec/32bit/memory_map.cc
SRC_CC += spec/arm/cpu.cc
SRC_CC += spec/arm/kernel/thread.cc
SRC_CC += spec/arm/platform_support.cc

# add assembly sources
SRC_S += spec/arm/crt0.s
SRC_S += spec/arm/exception_vector.s

vpath spec/32bit/memory_map.cc $(BASE_DIR)/../base-hw/src/lib/hw

# include less specific configuration
include $(BASE_DIR)/../base-hw/lib/mk/core-hw.inc
