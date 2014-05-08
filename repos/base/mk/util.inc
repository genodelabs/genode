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
# the same.
#
define _capture
$(eval $2 += $1)
$1
endef

#
# Lookup port directory by a given port name
#
# The location of the port directory is determined by the hash value stored
# in the corresponding 'ports/<port>.hash' file. First we have to find the
# hash file in one of the REPOSITORIES. Once we found the hash file, we use
# its contained hash number to construct the path to the corresponding
# subdirectory within CONTRIB_DIR. Finally, we check if the path exists.
#
# When reading the hash file in the '_hash_of_port' function, we feed stdin
# to 'cat' to prevent 'cat' from blocking if the hash file is missing.
#
# As a side effect of calling 'select_from_ports' we log the used hash file
# in the 'PORT_HASH_FILES' variable. This enables us incorporate the hash file
# as dependency for all object files.
#
_lookup_port_hash_file  = $(wildcard $(addsuffix /ports/$1.hash,$(REPOSITORIES)))
_capture_port_hash_file = $(call _capture,$(call _lookup_port_hash_file,$1),PORT_HASH_FILES)
_hash_of_port           = $(shell echo | cat $(call _capture_port_hash_file,$1))
_port_dir               = $(wildcard $(CONTRIB_DIR)/$1-$(call _hash_of_port,$1))
_checked_port_dir       = $(call _assert,$(call _port_dir,$1),$1 is not prepared or outdated)

select_from_ports = $(call _checked_port_dir,$1)
