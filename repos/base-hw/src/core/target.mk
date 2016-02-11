#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \author Sebastian Sumpf
# \date   2011-12-16
#

# set target name that this configuration applies to
TARGET = core

# library that provides the whole configuration
LIBS += core

# add C++ sources
SRC_CC += kernel/test.cc

ifneq ($(filter riscv, $(SPECS)),)
LD_TEXT_ADDR = $(CORE_LD_TEXT_ADDR)
endif
