#
# Specifics for the Linux-specific Genode components
#

#
# Startup code to be used when building a program and linker script that is
# specific for Linux. We also reserve the stack area via a segment in
# the program under Linux to prevent clashes with vdso.
#
ifneq ($(USE_HOST_LD_SCRIPT),yes)
LD_TEXT_ADDR     ?= 0x01000000
LD_SCRIPT_STATIC  = $(call select_from_repositories,src/ld/genode.ld) \
                    $(call select_from_repositories,src/ld/stack_area.nostdlib.ld)
else
LD_TEXT_ADDR     ?=
LD_SCRIPT_STATIC ?=
endif

ifneq ($(filter hardening_tool_chain, $(SPECS)),)
CC_OPT += -fPIC
endif
