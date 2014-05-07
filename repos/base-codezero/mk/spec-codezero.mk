#
# Specifics for the Codezero kernel API
#

#
# Read default and builddir-specific config files
#
# In these config files, we expect to find the definition of CODEZERO_DIR
#
-include $(call select_from_repositories,etc/codezero.conf)
-include $(BUILD_BASE_DIR)/etc/codezero.conf

ifeq ($(CODEZERO_DIR),)
$(error Could not find the definition of CODEZERO_DIR in etc/codezero.conf)
endif

#
# Convert path to absolute directory
#
absdir = $(shell readlink -f $(1))

#
# Headers generated within the build directory
# (see 'lib/mk/platform.mk')
#
INC_DIR += $(BUILD_BASE_DIR)/include

#
# Codezero headers
#
CODEZERO_ABS_DIR = $(call absdir,$(CODEZERO_DIR))

INC_DIR += $(CODEZERO_ABS_DIR)/include
INC_DIR += $(CODEZERO_ABS_DIR)/conts/userlibs/libl4/include
INC_DIR += $(CODEZERO_ABS_DIR)/conts/userlibs/libdev/uart/include

#
# Allow programs to test for the Codezero kernel
#
# This is needed by the 'pl050/irq_handler.h' to handle the interrupt semantics
# of Codezero.
#
CC_OPT += -D__CODEZERO__

#
# Clean rules for removing the side effects of building the platform
#
clean_includes:
	$(VERBOSE)rm -rf $(BUILD_BASE_DIR)/include

cleanall: clean_includes
