#
# Utility for selecting files from the list of repositories
#
select_from_repositories = $(firstword $(foreach REP,$(REPOSITORIES),$(wildcard $(REP)/$(1))))

#
# Check presence of argument $1. Back out with error message $2 if not defined.
#
_assert = $(if $1,$1,$(error Error: $2))

#
# Append value $1 to variable named $2 and return value $1
#
# We must not specify an '=' here. Even though the make documentation states
# that the omission of '=' should be equivalent to '=', the behaviour is not
# the same. Note, the result of the 'eval' function is always the empty string
# and, thus, it can be placed virtually anywhere in a makefile.
#
define _capture
$(eval $2 += $1)$1
endef

#
# Utility to read content from a file if it exists and the given file name
# is not empty.
#
_file_content = $(if $(wildcard $1),$(shell cat $1),)

#
# Lookup port directory by a given port name
#
# The location of the port directory is determined by the hash value stored
# in the corresponding 'ports/<port>.hash' file. First we have to find the
# hash file in one of the REPOSITORIES. Once we found the hash file, we use
# its contained hash number to construct the path to the corresponding
# subdirectory within CONTRIB_DIR. Finally, we check if the path exists.
#
# As a side effect of calling 'select_from_ports' we log the used hash file
# in the 'PORT_HASH_FILES' variable. This enables us incorporate the hash file
# as dependency for all object files.
#
_lookup_port_hash_file  = $(wildcard $(addsuffix /ports/$1.hash,$(REPOSITORIES)))
_capture_port_hash_file = $(call _capture,$(call _lookup_port_hash_file,$1),PORT_HASH_FILES)
_hash_of_port           = $(call _file_content,$(call _capture_port_hash_file,$1))
_port_dir               = $(wildcard $(CONTRIB_DIR)/$1-$(call _hash_of_port,$1))

#
# We accumulate missing ports instead of stopping only if the environment
# explicitely states that it is aware of it. This ensures that environments
# that are not aware can't run into troubles with missing ports.
#
ifeq (ACCUMULATE_MISSING_PORTS,)
_checked_port_dir = $(call _assert,$(call _port_dir,$1),$1 is not prepared or outdated)
else
_port_missing     = $(eval DEP_MISSING_PORTS += $1)
_checked_port     = $(if $2,$2,$(call _port_missing,$1))
_checked_port_dir = $(call _checked_port,$1,$(call _port_dir,$1))
endif

#
# Search port within the 'CONTRIB_DIR' if defined, which is the case when
# using the build system in the regular way. If the build system is invoked
# by the package-build tool, however, all source codes are - by definition -
# part of the package. In this case, 'CONTRIB_DIR' is undefined and we can
# simply return the package's 'REP_DIR'.
#
ifneq ($(CONTRIB_DIR),)
select_from_ports = $(call _checked_port_dir,$1)
else
select_from_ports = $(REP_DIR)
endif

#
# Utility to check for availability of a tool
#
check_tool = $(if $(shell command -v $(1)),,$(error Need to have '$(1)' installed.))
