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

#
# On RISCV we need a link address for core that differs from that of the other
# components.
#
ifneq ($(filter riscv, $(SPECS)),)
LD_TEXT_ADDR = $(CORE_LD_TEXT_ADDR)
endif

#
# We do not have additional source files than the core library
# so we need to define a dummy compilation unit,
# otherwise our build-system won't link
#
SRC_C += dummy.cc

dummy.cc:
	$(VERBOSE)touch $@
