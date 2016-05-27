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
%.o: %.adb
	$(MSG_COMP)$@
	$(VERBOSE)gnatmake -q -c $(CC_ADA_OPT) $(INCLUDES) $<

#
# Compiling Rust sources
#
%.o: %.rs
	$(MSG_COMP)$@
	$(VERBOSE)rustc $(CC_RUSTC_OPT) -o $@ $<

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
