#
# Specifics for the pistachio kernel API
#

#
# Read default and builddir-specific config files
#
# In these config files, we expect to find the definition of PISTACHIO_USER_BUILD_DIR
#
-include $(call select_from_repositories,etc/pistachio.conf)
-include $(BUILD_BASE_DIR)/etc/pistachio.conf

PISTACHIO_USER_BUILD_DIR ?= $(BUILD_BASE_DIR)/l4

#
# Pistachio headers
#
INC_DIR += $(PISTACHIO_USER_BUILD_DIR)/include

#
# Pistachio-specific Genode headers
#
REP_INC_DIR += include/pistachio

#
# Linker options
#
CXX_LINK_OPT += -L$(PISTACHIO_USER_BUILD_DIR)/lib
EXT_OBJECTS  += -ll4

clean_contrib:
	$(VERBOSE)rm -rf $(BUILD_BASE_DIR)/l4

cleanall: clean_contrib

