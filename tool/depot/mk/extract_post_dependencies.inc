#
# Obtain version information from recipes
#
# The 'archive_curr_version' function takes the archive type and name as
# arguments and returns the version identifier as present in the corresponding
# recipe. The nested foreach loop populates 'ARCHIVE_VERSION' with the version
# identifier for each archive.
#
# If an archive is given with a complete (versioned) name, we don't need to
# consult any recipe but only check if the corresponding archive exists within
# the depot. For binary archives, it suffices that the corresponding source
# archive is present.
#

$(foreach TYPE,api src raw pkg,\
   $(foreach PATH,${ARCHIVES(${TYPE})},\
      $(eval ARCHIVE_VERSION(${PATH}) := $(call archive_curr_version,$(PATH)))))

archive_exists_in_depot = $(wildcard $(DEPOT_DIR)/$1)

ARCHIVES_WITH_NO_VERSION := $(sort \
                               $(foreach TYPE,api src raw pkg,\
                                  $(foreach A,${ARCHIVES(${TYPE})},\
                                    $(if $(call archive_exists_in_depot,$A),,\
                                       $(if ${ARCHIVE_VERSION($A)},,$A)))))

checked_versions_defined:
ifneq ($(ARCHIVES_WITH_NO_VERSION),)
	@echo "Error: incomplete or missing recipe ($(sort $(ARCHIVES_WITH_NO_VERSION)))"; false
endif

#
# Read content of a file as list, sort it and remove duplicates
#
# \param $1  absolute file path
#
sorted_file_content = $(if $(wildcard $1),$(shell cat $1 | sort -u),\
                                          $(error Failed to read file $1))

#
# Absolute path to content.mk file for given archive
#
# \param $1  archive type, can be 'src', 'api' or 'raw'
# \param $2  api, src or raw archive name in the form 'genodelabs/api/base'
#
content_mk_file = $(addsuffix /content.mk,$(call recipe_dir,$1/$(call archive_recipe,$2)))

#
# Return a given string minus another given string
#
# \param $1  string that shall be returned minus the other one
# \param $2  string that shall be removed from the other one
#
remove_from_string=$(1:$2=)

#
# Repository directory for a given recipe name
#
# \param $1  recipe type in the form 'api'
# \param $2  recipe name in the form 'base'
#
rep_dir_of_recipe=$(call remove_from_string,$(call recipe_dir,$1/$2),/recipes/$1/$2)

#
# Repository directory for a given archive
#
# \param $1  archive type in the form 'api'
# \param $2  archive name in the form 'genodelabs/api/base'
#
rep_dir_of_archive=$(call rep_dir_of_recipe,$1,$(call archive_recipe,$2))

# path to temporary file that is used to buffer the names of missing ports
MISSING_PORTS_FILE := $(DEPOT_DIR)/var/missing_ports

# path to temporary make file that is created to fill the missing-ports file
GEN_MISSING_PORTS_MK := $(DEPOT_DIR)/var/gen_missing_ports.mk

# wether to invoke sub-makes with '--quiet'
QUIET = $(if $(VERBOSE),--quiet)

#
# Invoke sub-make to create or update missing-ports file
#
update_missing_ports_file: checked_versions_defined checked_no_uncategorized
	$(VERBOSE)mkdir -p $(dir $(GEN_MISSING_PORTS_MK))
	$(VERBOSE)( echo -e "all:\n"; \
	            echo -e "MAKE := $(MAKE)\n"; \
	            $(foreach TYPE,api src raw pkg,\
	                $(foreach A,${ARCHIVES(${TYPE})},\
	                   target=$(call versioned_archive,$A); \
	                   content_mk=$(call content_mk_file,$(TYPE),$A); \
	                   rep_dir=$(call rep_dir_of_archive,$(TYPE),$A); \
	                   echo "ARCHIVES(${TYPE}) += $$target"; \
	                   echo "CONTENT_MK($$target) := $$content_mk"; \
	                   echo "REP_DIR($$target) := $$rep_dir"; \
	            ) ) \
	            echo -e ""; \
	            $(foreach A,${ARCHIVES(pkg)},\
	               $(foreach DEP,$(call pkg_pkg_archives,$A),\
	                  echo -e "$(call versioned_archive,$A) :" \
	                          "$(call versioned_archive,$(DEP))";)) \
	            echo -e ""; \
	            echo -e "\$${ARCHIVES(src)} : \$${ARCHIVES(api)}"; \
	            echo -e "\$${ARCHIVES(pkg)} : \$${ARCHIVES(api)}"; \
	            echo -e "\$${ARCHIVES(pkg)} : \$${ARCHIVES(src)}"; \
	            echo -e "\$${ARCHIVES(pkg)} : \$${ARCHIVES(raw)}"; \
	            echo -e "\nTARGETS := \$$(foreach T,api src raw,\$${ARCHIVES(\$$T)})"; \
	            echo -e "\nall: \$$(TARGETS)"; \
	            echo -e "\n\$$(TARGETS):"; \
	            echo -e "\t$(VERBOSE)\$$(MAKE) \\"; \
	            echo -e "\t   $(QUIET) \\"; \
	            echo -e "\t   -f $(GENODE_DIR)/tool/depot/mk/content_env_missing_ports.mk \\"; \
	            echo -e "\t   GENODE_DIR=$(GENODE_DIR) \\"; \
	            echo -e "\t   CONTRIB_DIR=$(GENODE_DIR)/contrib \\"; \
	            echo -e "\t   CONTENT_MK=\$${CONTENT_MK(\$$@)} \\"; \
	            echo -e "\t   REP_DIR=\$${REP_DIR(\$$@)} \\"; \
	            echo -e "\t   MISSING_PORTS_FILE=$(MISSING_PORTS_FILE) \\"; \
	            echo -e "\t   VERBOSE=$(VERBOSE)"; \
	          ) > $(GEN_MISSING_PORTS_MK)
	$(VERBOSE)mkdir -p $(dir $(MISSING_PORTS_FILE))
	$(VERBOSE)rm -f $(MISSING_PORTS_FILE)
	$(VERBOSE)touch $(MISSING_PORTS_FILE)
	$(VERBOSE)$(MAKE) $(QUIET) -C $(DEPOT_DIR) -f $(GEN_MISSING_PORTS_MK);
