#
# 'GENODE_DIR' must be defined before including this file
#

TARGET := $(firstword $(sort $(MAKECMDGOALS)))
.PHONY: $(TARGET)

include $(GENODE_DIR)/tool/depot/mk/common.inc

# list of all repositories located at '<genode-dir>/repos/'
REPOSITORIES ?= $(shell find $(GENODE_DIR)/repos -follow -mindepth 1 -maxdepth 1 -type d)

# list of all repositories that contain depot recipes
REP_RECIPES_DIRS := $(wildcard $(addsuffix /recipes,$(REPOSITORIES)))

DEPOT_DIR := $(GENODE_DIR)/depot

usage:
	@$(ECHO) "$(subst $(NEWLINE),\n,$(HELP_MESSAGE))";


#
# Helper functions
#

# function for looking up a recipe directory from one of the repositories
recipe_dir = $(wildcard $(addsuffix /$1,$(REP_RECIPES_DIRS)))

# function for returning the archive version as given in the recipe
recipe_version = $(firstword $(call file_content,$(addsuffix /hash,$(call recipe_dir,$1))))


#
# Accessor functions for various elements of archive paths
#

sanitized = $(subst ..,__,$1)

path_element      = $(call sanitized,$(word $1,$(subst /, ,$2)))
last_path_element = $(call sanitized,$(lastword $(subst /, ,$1)))

archive_user     = $(call path_element,1,$1)
archive_type     = $(call path_element,2,$1)
archive_recipe   = $(call path_element,3,$1)
archive_has_type = $(filter $(call archive_type,$1),$2)
archive_has_user = $(filter $(call archive_user,$1),$2)
archive_version  = $(call recipe_version,$(addprefix $(call archive_type,$1)/,$(call archive_recipe,$1)))

# binary archives have the form <user>/bin/<spec>/<name>{-<version>}
bin_archive_spec    = $(call path_element,3,$1)
bin_archive_recipe  = $(call last_path_element,$1)
bin_archive_version = $(call recipe_version,src/$(call bin_archive_recipe,$1))

grep_archive_type = $(foreach A,$2,$(if $(call archive_has_type,$A,$1),$A,))
grep_archive_user = $(foreach A,$2,$(if $(call archive_has_user,$A,$1),$A,))

# return pkg-archive path of given binary-pkg archive path
pkg_of_binpkg = $(call archive_user,$1)/pkg/$(call bin_archive_recipe,$1)

# return source-archive path for given binary-archive path
src_of_bin = $(call archive_user,$1)/src/$(call bin_archive_recipe,$1)

# return binary-package archive path for architecture $1 and package archive $2
binpkg_for_pkg = $(call archive_user,$2)/bin/$1/$(call archive_recipe,$2)

