#
# Create prerequisites for building Genode for Codezero
#

#
# Execute the rules in this file only at the second build stage when we know
# about the complete build settings, e.g., the 'CROSS_DEV_PREFIX'.
#
ifeq ($(called_from_lib_mk),yes)

include $(REP_DIR)/lib/mk/codezero_cml.inc

all: $(BUILD_BASE_DIR)/include/l4/config.h

$(BUILD_BASE_DIR)/include/l4/config.h: $(CODEZERO_CML)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)$(CODEZERO_DIR)/tools/cml2header.py -i $^ -o $@

#
# Codezero's 'macros.h' includes the file "config.h", expected to be located in
# the same directory (using #include "config.h"). However, 'config.h' is
# generated into the source tree by the Codezero configuration system. Since we
# do not want to pollute the source tree, we create a shadow copy of 'macros.h'
# in the same directory as our generated 'config.h'.
#
all: $(BUILD_BASE_DIR)/include/l4/macros.h

$(BUILD_BASE_DIR)/include/l4/macros.h: $(CODEZERO_DIR)/include/l4/macros.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s $^ $@

endif

