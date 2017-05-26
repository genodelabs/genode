#
# Generic rules to build file types from other file types and other
# common functionaly that is needed to build library or program targets.
#

#
# Collect object files and avoid duplicates (by using 'sort')
#
SRC_O  += $(addprefix binary_,$(addsuffix .o,$(notdir $(SRC_BIN))))
SRC     = $(sort $(SRC_C) $(SRC_CC) $(SRC_ADA) $(SRC_RS) $(SRC_S) $(SRC_O))
OBJECTS = $(addsuffix .o,$(basename $(SRC)))

#
# Create sub directories for objects files corresponding to the sub directories
# of their respective source files
#
SUB_DIRS = $(sort $(dir $(OBJECTS)))
ifneq ($(SUB_DIRS),./)
$(OBJECTS): $(filter-out $(wildcard $(SUB_DIRS)), $(SUB_DIRS))
endif

.PHONY: $(SUB_DIRS)
$(SUB_DIRS):
	$(VERBOSE)mkdir -p $@

#
# Make sure that we rebuild object files and host tools after Makefile changes
#
$(wildcard $(OBJECTS)) $(HOST_TOOLS): $(filter-out $(LIB_PROGRESS_LOG),$(MAKEFILE_LIST))

INCLUDES := $(addprefix -I,$(wildcard $(ALL_INC_DIR)))

#
# If one of the 3rd-party ports used by the target changed, we need to rebuild
# all object files and host tools because they may include sources from the
# 3rd-party port.
#
# The 'PORT_HASH_FILES' variable is populated as side effect of calling the
# 'select_from_ports' function.
#
$(OBJECTS) $(HOST_TOOLS): $(PORT_HASH_FILES)

#
# Include dependency files for the corresponding object files except
# when cleaning
#
ifneq ($(filter-out $(MAKECMDGOALS),clean),)
-include $(OBJECTS:.o=.d)
endif

%.o: %.c
	$(MSG_COMP)$@
	$(VERBOSE)$(CC) $(CC_DEF) $(CC_C_OPT) $(INCLUDES) -c $< -o $@

%.o: %.cc
	$(MSG_COMP)$@
	$(VERBOSE)$(CXX) $(CXX_DEF) $(CC_CXX_OPT) $(INCLUDES) -c $< -o $@

%.o: %.cpp
	$(MSG_COMP)$@
	$(VERBOSE)$(CXX) $(CXX_DEF) $(CC_CXX_OPT) $(INCLUDES) -c $< -o $@

%.o: %.s
	$(MSG_ASSEM)$@
	$(VERBOSE)$(CC) $(CC_DEF) $(CC_C_OPT) $(INCLUDES) -c $< -o $@

#
# Compiling Ada source codes
#
# The mandatory runtime directories 'adainclude' and 'adalib' are expected in
# the program directory.
#
%.o: %.adb
	$(MSG_COMP)$@
	$(VERBOSE)$(GNATMAKE) -q -c --GCC=$(CC) --RTS=$(PRG_DIR) $< -cargs $(CC_ADA_OPT) $(INCLUDES)

#
# Compiling Rust sources
#
%.rlib: %.rs
	$(MSG_COMP)$@
	$(VERBOSE)rustc $(CC_RUSTC_OPT) --crate-type rlib -o $@ $<

%.o: %.rlib
	$(MSG_CONVERT)$@
	$(VERBOSE)ar p $< $*.0.o > $@

#
# Compiling Nim source code
#
ifneq ($(SRC_NIM),)

ifeq ($(NIM_CPU),)
$(warning NIM_CPU not defined for any of the following SPECS: $(SPECS))
else

NIM_MAKEFILES := $(foreach X,$(SRC_NIM),$(X).mk)
NIM_ARGS  = --compileOnly --os:genode --cpu:$(NIM_CPU)
NIM_ARGS += --verbosity:0 --hint[Processing]:off --nimcache:.
NIM_ARGS += $(NIM_OPT)

# Generate the C++ sources and compilation info
#
# Unfortunately the existing sources must be purged
# because of comma problems in the JSON recipe
%.nim.mk: %.nim
	$(MSG_BUILD)$(basename $@).cpp
	$(VERBOSE) rm -f stdlib_*.cpp
	$(VERBOSE)$(NIM) compileToCpp $(NIM_ARGS) $<
	$(VERBOSE)$(JQ) --raw-output '"SRC_O_NIM +=" + (.link | join(" ")) +"\n" + (.compile | map((.[0] | sub("cpp$$";"o: ")) + .[0] + "\n\t"+(.[1] | sub("^g\\++";"$$(MSG_COMP)$$@\n\t$$(VERBOSE)$$(NIM_CC)"))) | join("\n"))'  < $(basename $(basename $@)).json > $@

NIM_CC := $(CXX) $(CXX_DEF) $(CC_CXX_OPT) $(INCLUDES) -D__GENODE__

# Parse the generated makefiles
-include $(NIM_MAKEFILES)

# Append the new objects
SRC_O += $(sort $(SRC_O_NIM))

endif
endif

#
# Assembler files that must be preprocessed are fed to the C compiler.
#
%.o: %.S
	$(MSG_COMP)$@
	$(VERBOSE)$(CC) $(CC_DEF) $(CC_OPT) -D__ASSEMBLY__ $(INCLUDES) -c $< -o $@

#
# Link binary data
#
# We transform binary data into an object file by using the 'incbin' directive
# of the GNU assembler. This enables us to choose a any label for the binary
# data (in contrast to 'ld -r -oformat default -b binary', which generates the
# label from the input path name) and to align the binary data as required on
# some architectures (e.g., ARM).
#
symbol_name = _binary_$(subst -,_,$(subst .,_,$(subst binary_,,$(subst .o,,$(notdir $@)))))

binary_%.o: %
	$(MSG_CONVERT)$@
	$(VERBOSE)echo ".global $(symbol_name)_start, $(symbol_name)_end; .data; .align 4; $(symbol_name)_start:; .incbin \"$<\"; $(symbol_name)_end:" |\
		$(AS) $(AS_OPT) -f -o $@ -

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

%.symbols.s: %.symbols
	$(MSG_CONVERT)$@
	$(VERBOSE)\
		sed -e "s/^\(\w\+\) D \(\w\+\)\$$/.data; .global \1; .type \1,%object; .size \1,\2; \1:/p" \
		    -e "s/^\(\w\+\) V/.data; .weak \1; .type \1,%object; \1:/p" \
		    -e "s/^\(\w\+\) T/.text; .global \1; .type \1,%function; \1:/p" \
		    -e "s/^\(\w\+\) R/.section .rodata; .global \1; \1:/p" \
		    -e "s/^\(\w\+\) W/.text; .weak \1; .type \1,%function; \1:/p" \
		    -e "s/^\(\w\+\) B \(\w\+\)\$$/.bss; .global \1; .type \1,%object; .size \1,\2; \1:/p" \
		    -e "s/^\(\w\+\) U/.text; .global \1; $(ASM_SYM_DEPENDENCY)/p" \
		    $< > $@

#
# Create local symbol links for the used shared libraries
#
# Depending on whether an ABI stub for a given shared library exists, we link
# the target against the ABI stub or the real shared library.
#
# We check if the symbolic links are up-to-date by filtering all links that
# already match the current shared library targets from the list. If the list
# is not empty we flag 'SHARED_LIBS' as phony to make sure that the symbolic
# links are recreated. E.g., if a symbol list is added for library, the next
# time a user of the library is linked, the ABI stub should be used instead of
# the library.
#
select_so = $(firstword $(wildcard $(LIB_CACHE_DIR)/$(1:.lib.so=)/$(1:.lib.so=).abi.so \
                                   $(LIB_CACHE_DIR)/$(1:.lib.so=)/$(1:.lib.so=).lib.so))

ifneq ($(filter-out $(foreach s,$(SHARED_LIBS),$(realpath $s)), \
                    $(foreach s,$(SHARED_LIBS),$(call select_so,$s))),)
.PHONY: $(SHARED_LIBS)
endif
$(SHARED_LIBS):
	$(VERBOSE)ln -sf $(call select_so,$@) $@
