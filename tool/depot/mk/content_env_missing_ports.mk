#
# \brief  Environment for content.mk files when determining missing ports
# \author Martin Stein
# \date   2019-05-12
#
# GENODE_DIR         - root directory of the Genode source tree
# CONTRIB_DIR        - directory for 3rd-party code
# CONTENT_MK         - content.mk file to process
# REP_DIR            - repository directory of the content.mk file
# MISSING_PORTS_FILE - file to write the names of missing ports to
# VERBOSE            - verbosity
#

#
# Functions for disabling and re-enabling evaluation of $(shell ...)
#
ORIGINAL_SHELL := $(SHELL)
enable_shell  = $(eval SHELL:=$(ORIGINAL_SHELL))
disable_shell = $(eval SHELL:=true)

#
# Disable shell calls so the content.mk file will not evaluate something like
# $(shell find $(PORT_DIR) ...) while 'PORT_DIR' is empty because we have
# overridden the port_dir function.
#
$(disable_shell)

#
# If a port is missing, append its name to the missing ports file
#
_assert = $(if $1,$1,$(shell echo $2 >> $(MISSING_PORTS_FILE)))

#
# Utility to query the port directory for a given path to a port description.
#
# Example:
#
#   $(call port_dir,$(GENODE_DIR)/repos/libports/ports/libpng)
#
_port_hash = $(shell cat $(call _assert,$(wildcard $1.hash),$(notdir $1)))
_port_dir  = $(wildcard $(CONTRIB_DIR)/$(notdir $1)-$(call _port_hash,$1))
port_dir   = $(call enable_shell)$(call _assert,$(call _port_dir,$1),$(notdir $1))$(call disable_shell)

#
# Prevent the evaluation of mirror_from_rep_dir in content.mk
#
mirror_from_rep_dir = $(error mirror_from_rep_dir called outside of target)

#
# Prevent the evaluation of the first target in the content.mk file
#
prevent_execution_of_content_targets:

#
# Include the content.mk file to evaluate all calls to the port_dir function
#
include $(CONTENT_MK)
