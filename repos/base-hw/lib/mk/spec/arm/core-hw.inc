#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
REP_INC_DIR += src/core/spec/arm

LIBS += syscall-hw

# add C++ sources
SRC_CC += spec/32bit/memory_map.cc
SRC_CC += spec/arm/kernel/cpu.cc
SRC_CC += spec/arm/kernel/pd.cc
SRC_CC += spec/arm/cpu.cc
SRC_CC += spec/arm/kernel/thread.cc
SRC_CC += spec/arm/platform_support.cc

# add assembly sources
SRC_S += spec/arm/crt0.s
SRC_S += spec/arm/exception_vector.S

vpath spec/32bit/memory_map.cc $(call select_from_repositories,src/lib/hw)

# include less specific configuration
include $(call select_from_repositories,lib/mk/core-hw.inc)
