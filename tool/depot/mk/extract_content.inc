#
# \brief  Rule for populating an archive by evaluating a content.mk file
# \author Norman Feske
# \date   2017-03-17
#
# This file complements 'create.inc' for the creation of API and source
# archives.
#
# Arguments:
#
#   RECIPE_DIR        - location of the recipe for the source or API archive
#   DEPOT_ARCHIVE_DIR - archive destination within the depot
#   TAG_FILE          - file within archive used as tag for make dependencies
#   GENODE_DIR        - root of the Genode source tree
#   REP_DIR           - source repository where 'RECIPE_DIR' is located
#

#
# Validate existance of a content.mk file for the archive recipe
#
ifneq ($(RECIPE_DIR),)
EXPECTED_CONTENT_MK_FILE := $(RECIPE_DIR)/content.mk
CONTENT_MK_FILE          := $(wildcard $(EXPECTED_CONTENT_MK_FILE))
endif

checked_content_mk_exists: checked_recipe_is_unique
ifeq ($(CONTENT_MK_FILE),)
	@$(ECHO) "Error: Recipe misses content.mk file,\n" \
	          "      expected at '$(EXPECTED_CONTENT_MK_FILE)'"; false
endif

$(TARGET) $(DEPOT_ARCHIVE_DIR)/$(TAG_FILE): checked_content_mk_exists

#
# Handle the creation of the TAG_FILE (and the population of the archive
# directory as its side effect) as an atomic step. Otherwise a dependent
# rule - in particular the rule for generating the hash - could be
# triggered as soon as the specific tag file appears but before the entire
# sub make is finished with populating the archive directory.
#
.NOTPARALLEL: $(DEPOT_ARCHIVE_DIR)/$(TAG_FILE)

#
# Assemble archive content by invoking the recipe's content.mk file
#
# If an error (such as a missing installation of a port) occurs during this
# step, remove the incomplete archive before returning an error.
#
QUIET := $(if $(VERBOSE),--quiet)
$(DEPOT_ARCHIVE_DIR)/$(TAG_FILE): $(CONTENT_MK_FILE)
	$(VERBOSE)$(MAKE) $(QUIET) -f $(GENODE_DIR)/tool/depot/mk/content_env.mk \
	                           -C $(DEPOT_ARCHIVE_DIR) \
	                           CONTENT_MK=$< \
	                           GENODE_DIR=$(GENODE_DIR) \
	                           REP_DIR=$(REP_DIR) \
	                           CONTRIB_DIR=$(GENODE_DIR)/contrib || \
	( rm -r $(DEPOT_ARCHIVE_DIR); false )
	$(VERBOSE)find $(DEPOT_ARCHIVE_DIR) \( -name "*~" \
	                                   -or -name "*.rej" \
	                                   -or -name "*.orig" \
	                                   -or -name "*.swp" \) -delete

