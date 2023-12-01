##
## Create an ABI stub named '<lib>.abi.so'
##
## Invoked from the library's build directory within the lib cache.
## The following variables must be passed when calling this file:
##
##   BASE_DIR      - base directory of the build system
##   VERBOSE       - build verboseness modifier
##   LIB_CACHE_DIR - library build cache location
##   LIB           - library name
##   SYMBOLS       - path of symbols file
##   SPECS         - build specs (i.e., CPU architecture)
##

#
# An ABI-stub library does not contain any code or data but only the symbol
# information of the binary interface (ABI) of the shared library.
#
# The ABI stub is linked by the users of the library (executables or shared
# objects) instead of the real library. This effectively decouples the library
# users from the concrete library instance but binds them merely to the
# library's binary interface. Note that the ABI stub is not used at runtime at
# all. At runtime, the real library that implements the ABI is loaded by the
# dynamic linker.
#
# The symbol information are incorporated into the ABI stub via an assembly
# file named 'symbols.s' that is generated from the library's symbol list.
#

ABI_SO  := $(addsuffix .abi.so,$(LIB))
SYMBOLS := $(wildcard $(SYMBOLS))

ifeq ($(SYMBOLS),)
all:
else
all: $(ABI_SO)
endif

include $(BASE_DIR)/mk/util.inc
include $(SPEC_FILES)
include $(BASE_DIR)/mk/global.mk
include $(BASE_DIR)/mk/generic.mk

#
# Generate assembler file from symbol list
#
# For undefined symbols (type U), we create a hard dependency by referencing
# the symbols from the assembly file. The reference is created in the form of
# a '.long' value with the address of the symbol. On x86_64, this is not
# possible for PIC code. Hence, we reference the symbol via a PIC-compatible
# movq instruction instead.
#
# If we declared the symbol as '.global' without using it, the undefined symbol
# gets discarded at link time unless it is directly referenced by the target.
# This is a problem in situations where the undefined symbol is resolved by an
# archive rather than the target. I.e., when linking posix.lib.a (which
# provides 'Libc::Component::construct'), the 'construct' function is merely
# referenced by the libc.lib.so's 'Component::construct' function. But this
# reference apparently does not suffice to keep the posix.lib.a's symbol. By
# adding a hard dependency, we force the linker to resolve the symbol and don't
# drop posix.lib.a.
#
ASM_SYM_DEPENDENCY := .long \1
ifeq ($(filter-out $(SPECS),x86_64),)
ASM_SYM_DEPENDENCY := movq \1@GOTPCREL(%rip), %rax
endif

symbols.s: $(SYMBOLS)
	$(MSG_CONVERT)$@
	$(VERBOSE)\
		sed -e "s/^\(\w\+\) D \(\w\+\)\$$/.data; .global \1; .type \1,%object; .size \1,\2; \1: .skip 1/" \
		    -e "s/^\(\w\+\) V/.data; .weak \1; .type \1,%object; \1: .skip 1/" \
		    -e "s/^\(\w\+\) T/.text; .global \1; .type \1,%function; \1:/" \
		    -e "s/^\(\w\+\) R \(\w\+\)\$$/.section .rodata; .global \1; .type \1,%object; .size \1,\2; \1:/" \
		    -e "s/^\(\w\+\) W/.text; .weak \1; .type \1,%function; \1:/" \
		    -e "s/^\(\w\+\) B \(\w\+\)\$$/.bss; .global \1; .type \1,%object; .size \1,\2; \1:/" \
		    -e "s/^\(\w\+\) U/.text; .global \1; $(ASM_SYM_DEPENDENCY)/" \
		    $< > $@

#
# The '.PRECIOUS' special target prevents make to remove the intermediate
# assembler file. Otherwise make would spill the build log with messages
# like "rm libc.symbols.s".
#
.PRECIOUS: symbols.s

ABI_SONAME := $(addsuffix .lib.so,$(LIB))

all: # prevent 'Nothing to be done' message
	@true

$(ABI_SO): symbols.o
	$(MSG_MERGE)$(ABI_SO)
	$(VERBOSE)$(LD) -o $(ABI_SO) -soname=$(ABI_SONAME) -shared --eh-frame-hdr $(LD_OPT) \
	                -T $(LD_SCRIPT_SO) $<

$(ABI_SO): $(LD_SCRIPT_SO)
