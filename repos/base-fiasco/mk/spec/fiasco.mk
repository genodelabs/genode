#
# Specifics for the l4v2 kernel API
#

#
# Read default and builddir-specific config files
#
# In these config files, we find the definition of L4_DIR
#
-include $(call select_from_repositories,etc/fiasco.conf)
-include $(BUILD_BASE_DIR)/etc/fiasco.conf

L4_BUILD_DIR := $(BUILD_BASE_DIR)/l4
L4_SRC_DIR   := $(call select_from_ports,fiasco)/src/kernel/fiasco/fiasco/snapshot

#
# L4/sys headers
#
L4_INC_DIR   += $(L4_BUILD_DIR)/include \
                $(L4_BUILD_DIR)/include/l4v2

clean_contrib:
	$(VERBOSE)rm -rf $(BUILD_BASE_DIR)/l4

cleanall: clean_contrib
