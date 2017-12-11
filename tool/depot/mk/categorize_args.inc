#
# \brief  Initial processing of archive-path arguments
# \author Norman Feske
# \date   2017-03-21
#
# This include transforms an arbitrary number of user-specified arguments
# into a digestable representation in the form of the 'ARCHIVES' variables
# indexed by the archive type.
#

ARCHIVE_PATHS := $(sort $(MAKECMDGOALS))
ARCHIVE_TYPES := api src raw bin pkg


#
# Categorize argument paths into the different types of archives.
#
# The 'categorize_archives' function populates the 'ARCHIVES(<type>)' variables
# with the matching paths.
#
#

categorize_archives = $(foreach PATH,$(ARCHIVE_PATHS),\
                         $(if $(call archive_has_type,$(PATH),$1),\
                            $(eval ARCHIVES($1) += $(PATH))))

$(foreach TYPE,$(ARCHIVE_TYPES),$(call categorize_archives,$(TYPE)))

CATEGORIZED   := $(foreach TYPE,$(ARCHIVE_TYPES),${ARCHIVES(${TYPE})})
UNCATEGORIZED := $(filter-out $(CATEGORIZED),$(ARCHIVE_PATHS))

checked_no_uncategorized:
ifneq ($(UNCATEGORIZED),)
	@echo "Error: unknown archive type ($(UNCATEGORIZED))"; false
endif


#
# Sub-categorize source-pkg archives (<user>/pkg/<name>[/<version>]) from
# binary-pkg archives (<user>/pkg/<spec>/<name>,[<version>]) so that
# 'ARCHIVES(pkg)' contains source pkgs only, and 'ARCHIVES(binpkg)' contains
# binary pkgs.
#
# If the path contains 'BIN_PKG_PATH_ELEMS' elements, it refers to a binary pkg
# where the third element is the build spec. Otherwise, the path refers to a
# source pkg. By default, the distinction assumes versioned archive paths.
# For the 'extract' tool where the version elements are omitted because they
# refer to the current version as present in the repository, the value is
# customized to '4'.
#

BIN_PKG_PATH_ELEMS ?= 5

_src_pkg = $(if $(word $(BIN_PKG_PATH_ELEMS),$(subst /, ,$1)),,$1)
_bin_pkg = $(if $(word $(BIN_PKG_PATH_ELEMS),$(subst /, ,$1)),$1,)

ARCHIVES(binpkg) := $(strip $(foreach PKG,${ARCHIVES(pkg)},$(call _bin_pkg,$(PKG))))
ARCHIVES(pkg)    := $(strip $(foreach PKG,${ARCHIVES(pkg)},$(call _src_pkg,$(PKG))))
