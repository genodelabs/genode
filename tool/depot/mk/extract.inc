#
# \brief  Common steps of creating an archive within the depot
# \author Norman Feske
# \date   2017-03-17
#
# Variables that must be defined when including this file:
#
#   ARCHIVE         - archive name, corresponds to recipe name
#   RECIPE_DIR      - location of the archive recipe within the source tree
#   DEPOT_SUB_DIR   - archive-type-dependent destination within the depot
#   TAG_FILE        - file within the archive used as tag for make dependencies
#   UPDATE_VERSIONS - update recipe hash file if archive content changed
#


##
## Obtain information about the user-specified archive recipe
##
## Validate 'RECIPE_DIR', determine the version and content hash as given in
## the recipe, and define 'RECIPE_HASH_VALUE' and 'RECIPE_VERSION'. Rules using
## those definitions should depend on 'checked_recipe_hash_value_exists'.
##

checked_recipe_dir_specified:
ifeq ($(RECIPE_DIR),)
	@$(ECHO) "Error: recipe for '$(TARGET)' is missing"; false
endif

$(TARGET): checked_recipe_dir_specified

checked_recipe_is_unique: checked_recipe_dir_specified
ifneq ($(RECIPE_DIR),$(firstword $(RECIPE_DIR)))
	@echo "Error: recipe for '$(TARGET)' is ambiguous, candidates are:";\
	 for dir in $(RECIPE_DIR_CANDIDATES); do \
	   echo "       $$dir"; done; \
	 false
RECIPE_DIR_CANDIDATES := $(subst %,&,$(RECIPE_DIR))
RECIPE_DIR :=
endif

$(TARGET): checked_recipe_is_unique

#
# Determine hash file of current archive version as defined by the recipe
#

ifneq ($(RECIPE_DIR),)
EXPECTED_RECIPE_HASH_FILE := $(RECIPE_DIR)/hash
RECIPE_HASH_FILE          := $(wildcard $(EXPECTED_RECIPE_HASH_FILE))
endif

checked_recipe_hash_file_exists: checked_recipe_is_unique
ifeq ($(RECIPE_HASH_FILE),)
	@$(ECHO) "Error: Recipe hash file is missing,\n" \
	          "      expected at '$(EXPECTED_RECIPE_HASH_FILE)'"; false
endif

$(TARGET): checked_recipe_hash_file_exists

#
# Obtain hash value and version identifier from hash file. If no version
# identifier is present, the hash value is taken as version ('lastword' is
# the same as 'firstword'). This is the normal case for source archives.
#

ifneq ($(RECIPE_HASH_FILE),)
_RECIPE_HASH_FILE_CONTENT = $(call file_content,$(RECIPE_HASH_FILE))
RECIPE_HASH_VALUE         = $(lastword  $(_RECIPE_HASH_FILE_CONTENT))
RECIPE_VERSION            = $(firstword $(_RECIPE_HASH_FILE_CONTENT))
endif

checked_recipe_hash_value_exists:
ifeq ($(RECIPE_HASH_VALUE),)
	@$(ECHO) "Error: archive hash is undefined"; false
endif

#
# Remember content hash at invocation time, to detect a potential update caused
# by the 'UPDATE_VERSIONS' feature.
#
ORIG_RECIPE_HASH_VALUE := $(RECIPE_HASH_VALUE)

#
# Define name of temporary archive directory that we use until we know the
# archive hash.
#

DEPOT_ARCHIVE_DIR := $(DEPOT_SUB_DIR)/$(ARCHIVE).incomplete

reset_stale_temporary_archive_dir:
ifneq ($(wildcard $(DEPOT_ARCHIVE_DIR)),)
	$(VERBOSE)rm -rf $(DEPOT_ARCHIVE_DIR); mkdir -p $(DEPOT_ARCHIVE_DIR)
endif

$(DEPOT_ARCHIVE_DIR)/$(TAG_FILE): reset_stale_temporary_archive_dir


##
## Create archive
##

#
# Rename archive to the hashed name after successful completion
#
$(ARCHIVE): _rename_to_final_archive

#
# Rename archive name from the temporary name to the hashed name. However,
# if the hashed archive name already exists, keep the existing one and
# discard the just-built archive.
#
_rename_to_final_archive: _check_hash
	@$(VERBOSE)final_name=$(ARCHIVE)-$(RECIPE_VERSION); \
	           rm -rf $(DEPOT_SUB_DIR)/$$final_name; \
	           mv $(DEPOT_ARCHIVE_DIR) $(DEPOT_SUB_DIR)/$$final_name; \
	           hash=$$(< $(DEPOT_ARCHIVE_DIR).hash); hint=""; \
	           test $$hash = $(ORIG_RECIPE_HASH_VALUE) ||\
	             hint=" $(BRIGHT_COL)(new version)$(DEFAULT_COL)"; \
	           rm -f $(DEPOT_ARCHIVE_DIR).hash; \
	           $(ECHO) "$(DARK_COL)created$(DEFAULT_COL)" \
	                   "$(USER)/$(notdir $(DEPOT_SUB_DIR))/$$final_name$$hint"; \
	           true;

#
# Generate suggested version name for 'HASH_OUT_OF_DATE_MESSAGE'
#
# We suggest to use the current date as version. If there is already a version
# with the current date, we add a single-letter suffix to distinguish the new
# version. If we run out of letters or if the old recipe's version cannot be
# compared with the current date, we suggest to append a '-x' to the old
# recipe's version. In the latter case (e.g., when using a versioning scheme
# not based on dates), the package creator may want to manually adjust the
# version identifier anyway.
#
suffix_from_list = $(subst $(EMPTY) $(EMPTY),,$(strip $1))
suffixed_version = $1$(if $(call suffix_from_list,$2),-$(call suffix_from_list,$2),)

_string_higher_than = $(filter-out $(firstword $(sort $1 $2)),$1)
_higher_string      = $(if $(call _string_higher_than,$1,$2),$1,$2)

CURRENT_DATE = $(strip $(shell date --iso-8601))

_version_higher_than_recipe = $(call _string_higher_than,\
                                 $(call suffixed_version,$1,$2),$(RECIPE_VERSION))

_next_version = $(eval FOUND := $(if $(call _version_higher_than_recipe,$1,$2),\
                                   $(call suffixed_version,$1,$2)))\
                $(foreach C,a b c d e f g h i j k l m n o p q r s t u v w x y z,\
                   $(if $(FOUND),,\
                      $(if $(call _version_higher_than_recipe,$1,$2 $C),\
                           $(eval FOUND := $(call suffixed_version,$1,$2 $C)))))\
                $(if $(FOUND),$(FOUND),$(addsuffix -x,$(RECIPE_VERSION)))

next_version = $(strip $(call _next_version,$(CURRENT_DATE)))

#
# Message presented to the user whenever a recipe hash is out of date
#
define HASH_OUT_OF_DATE_MESSAGE

Error: $(RECIPE_HASH_FILE) is out of date.

This error indicates that the archived source code has changed, which should
be reflected by incrementing the archive version and updating the hash of
the recipe. You may update the recipe hash via the following command:

  echo" "$(next_version)" $$hash "> $(RECIPE_HASH_FILE)

The above command takes the current date as version identifier. Should this
not be your intention, please adjust the hash file manually.

endef


ifeq ($(UPDATE_VERSIONS),)
HASH_MISMATCH_CMD = $(ECHO) "$(subst $(NEWLINE),\n,$(HASH_OUT_OF_DATE_MESSAGE))"; false
else
HASH_MISMATCH_CMD = echo "$(next_version) $$hash" > $(RECIPE_HASH_FILE);
endif

#
# Check the consistency between the hash of the archive recipe and the actual
# hash of the generated archive. If both hash values differ, we print the
# expected hash value and remove the archive. The user is expected to
# update the recipe hash before attempting the archive creation again.
#
_check_hash: $(DEPOT_ARCHIVE_DIR).hash checked_recipe_hash_value_exists
	$(VERBOSE)hash=$$(< $(DEPOT_ARCHIVE_DIR).hash); \
	          test $$hash = $(RECIPE_HASH_VALUE) || ($(HASH_MISMATCH_CMD))

#
# Shell command used to calculate the hash of an archive
#
# The command is invoked from within the archive directory within the depot.
#
# The 'echo' right before 'cat' is needed to handle the special case where
# 'find' is executed in an empty directory (yielding an empty result), which
# would otherwise prompt 'cat' to block for standard input.
#
HASH_CMD := cd $(DEPOT_ARCHIVE_DIR); \
            echo | cat `find . -type f | LC_COLLATE=C sort` |  $(HASHSUM) | sed "s/ .*//" \

#
# Generate the hash from the archive content
#
$(DEPOT_ARCHIVE_DIR).hash: $(DEPOT_ARCHIVE_DIR)/$(TAG_FILE)
	$(VERBOSE)$(HASH_CMD) > $@

$(DEPOT_ARCHIVE_DIR):
	$(VERBOSE)mkdir -p $@

$(DEPOT_ARCHIVE_DIR)/$(TAG_FILE): $(DEPOT_ARCHIVE_DIR)

