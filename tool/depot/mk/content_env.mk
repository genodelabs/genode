#
# \brief  Environment for executing content.mk files
# \author Norman Feske
# \date   2006-05-13
#
# The file is executed from within the archive directory.
# The following variables must be defined by the caller:
#
# GENODE_DIR   - root directory of the Genode source tree
# CONTRIB_DIR  - collection of 3rd-party ports, usually $(GENODE_DIR)/contrib
# CONTENT_MK   - content.mk file to execute
# VERBOSE      - verbosity
#


#
# Check presence of argument $1. Back out with error message $2 if not defined.
#
_assert = $(if $1,$1,$(error Error: $2))


#
# Utility to query the port directory for a given path to a port description.
#
# Example:
#
#   $(call port_dir,$(GENODE_DIR)/repos/libports/ports/libpng)
#
_hash_of_port     = $(shell cat $(call _assert,$(wildcard $1.hash),$(notdir $1) port does not exist))
_port_dir         = $(wildcard $(CONTRIB_DIR)/$(notdir $1)-$(call _hash_of_port,$1))
_checked_port_dir = $(call _assert,$(call _port_dir,$1),$(notdir $1) port is not prepared or outdated)

port_dir = $(call _checked_port_dir,$1)


#
# Handy shortcuts to be used in content.mk files
#
mirror_from_rep_dir = mkdir -p $(dir $@); cp -r $(REP_DIR)/$@ $(dir $@)


#
# Execute recipe's content.mk rules for populating the archive directory
#
include $(CONTENT_MK)

