#
# Specifics for the OKL4 kernel API
#

#
# Read default and builddir-specific config files
#
-include $(call select_from_repositories,etc/okl4.conf)
-include $(BUILD_BASE_DIR)/etc/okl4.conf

#
# If no OKL4 source directory is set, we use the standard contrib directory.
# We do this with ifeq and := as ?= would be done lazy. Forcing the
# evaluation of $(call select_from_ports,okl4) ensures that the kernel
# port, if missing, is added to the missing-ports list of the first build
# stage.
#
ifeq ($(OKL4_DIR),)
OKL4_DIR := $(call select_from_ports,okl4)/src/kernel/okl4
endif

#
# Make sure that symlink modification times are handled correctly.
# Otherwise, the creation of symlinks that depend on their own directory
# behaves like a phony rule. This is because the directory mtime is
# determined by taking the mtimes of containing symlinks into account.
# Hence, all symlinks (except for the youngest) depend on a directory
# with a newer mtime. The make flag -L fixes the problem. Alternatively,
# we could use 'cp' instead of 'ln'.
#
MAKEFLAGS += -L

#
# OKL4-specific Genode headers
#
INC_DIR += $(BUILD_BASE_DIR)/include

#
# Define maximum number of threads, needed by the OKL4 kernel headers
#
CC_OPT += -DCONFIG_MAX_THREAD_BITS=10

#
# Clean rules for removing the side effects of building the platform
# library
#
clean_includes:
	$(VERBOSE)rm -rf $(BUILD_BASE_DIR)/include

clean_tool_elfweaver:
	$(VERBOSE)rm -rf $(BUILD_BASE_DIR)/tool/okl4/elfweaver

clean cleanall: clean_includes clean_tool_elfweaver
