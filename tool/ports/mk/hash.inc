#
# \brief  Utilities for generating and checking port hashes
# \author Norman Feske
# \date   2014-05-27
#

#
# The hash depends on the content of the port description file and the
# patches originating from REP_DIR. Patches that are downloaded are already
# captured by the hash sums of the downloaded files, which are declared in the
# port description file.
#
# During development when the files of MAKEFILE_LIST often change, the
# keeping all port hashes up to date is an inconvenience. By setting
# STRICT_HASH to 'no', the MAKEFILE_LIST can be exluded.
#

$(call check_tool,sha1sum)

_PATCHES_IN_REP_DIR := $(foreach P,$(PATCHES),\
                         $(wildcard $(REP_DIR)/$(subst $(REP_DIR)/,,$(P))))

HASH_INPUT += $(sort $(_PATCHES_IN_REP_DIR)) $(PORT)
ifneq ($(STRICT_HASH),no)
HASH_INPUT += $(MAKEFILE_LIST)
endif

$(_DST_HASH_FILE): $(HASH_INPUT) $(MAKEFILE_LIST)
	@$(MSG_GENERATE)$(notdir $@)
	$(VERBOSE)cat $(HASH_INPUT) | sha1sum | sed "s/ .*//" > $@

_check_hash: $(_DST_HASH_FILE)
ifneq ($(CHECK_HASH),no)
	$(VERBOSE)diff $< $(HASH_FILE) > /dev/null ||\
		($(ECHO) "Error: $(_REL_HASH_FILE) is out of date, expected" `cat $<` ""; false)
endif


