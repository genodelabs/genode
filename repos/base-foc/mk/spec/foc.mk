#
# Specifics for the Fiasco.OC kernel API
#

-include $(call select_from_repositories,etc/foc.conf)
-include $(BUILD_BASE_DIR)/etc/foc.conf

#
# L4/sys headers
#
L4_INC_DIR  += $(BUILD_BASE_DIR)/include/
L4F_INC_DIR += $(BUILD_BASE_DIR)/include/l4f

#
# L4 build directory, if not defined by 'foc.conf', use directory local
# to the Genode build directory.
#
L4_BUILD_DIR ?= $(BUILD_BASE_DIR)/l4

#
# Build everything with -fPIC because the Fiasco.OC syscall bindings
# rely on 'ebx' (on x86) being handled with care. Without -fPIC enabled,
# the syscall bindings break.
#
CC_OPT += -fPIC

#
# Use 'regparm=0' call instead of an inline function, when accessing
# the utcb. This is needed to stay compatible with L4linux
#
CC_OPT += -DL4SYS_USE_UTCB_WRAP=1

all:

#
# Clean rules for removing the side effects of building the platform
# library
#
clean_includes:
	$(VERBOSE)rm -rf $(BUILD_BASE_DIR)/include

clean_contrib:
	$(VERBOSE)rm -rf $(BUILD_BASE_DIR)/l4

cleanall: clean_contrib clean_includes

