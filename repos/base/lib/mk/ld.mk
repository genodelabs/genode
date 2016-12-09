#
# Generic ld.lib.so stub library
#
# This library is used to build kernel-independent dynamically linked
# executables. It does not contain any code or data but only the symbol
# information of the binary interface of the Genode API.
#
# Note that this library is not used as runtime at all. At system-integration
# time, it is transparently replaced by the variant of the dynamic linker that
# matches the used kernel.
#
# The rule for generating 'symbols.s' is defined in the architecture-dependent
# 'spec/<architecture>/ld.mk' file.
#

SRC_S := symbols.s

SYMBOLS := $(BASE_DIR)/lib/symbols/ld

SHARED_LIB := yes

LD_OPT += -Bsymbolic-functions --version-script=$(BASE_DIR)/src/lib/ldso/symbol.map
LD_OPT += -T$(BASE_DIR)/src/lib/ldso/linker.ld

symbols.s: $(MAKEFILE_LIST)

symbols.s: $(SYMBOLS)
	$(MSG_CONVERT)$@
	$(VERBOSE)\
		sed -e "s/^\(\w\+\) D \(\w\+\)\$$/.data; .global \1; .type \1,%object; .size \1,\2; \1:/p" \
		    -e "s/^\(\w\+\) V/.data; .weak \1; .type \1,%object; \1:/p" \
		    -e "s/^\(\w\+\) T/.text; .global \1; .type \1,%function; \1:/p" \
		    -e "s/^\(\w\+\) R/.section .rodata; .global \1; \1:/p" \
		    -e "s/^\(\w\+\) W/.text; .weak \1; .type \1,%function; \1:/p" \
		    -e "s/^\(\w\+\) B/.bss; .global \1; .type \1,%object; \1:/p" \
		    $(SYMBOLS) > $@
