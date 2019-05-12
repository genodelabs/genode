#
# Collect dependencies for all specified arguments
#
# The following accessor functions used by 'mk/dependencies.inc'. The
# information found in the 'archives' file of a package recipe has the
# placeholder '_' for the user. Only archives with this placeholder are
# considered. The '_user_pkg_archives' function transforms those archive paths
# into user-specific archive paths.
#

_file_in_depot  = $(wildcard $(DEPOT_DIR)/$(call archive_user,$1)/src/$(call archive_recipe,$1)/$2)
_file_in_recipe = $(addsuffix /$2,$(call recipe_dir,src/$(call archive_recipe,$1)))

_file_in_depot_or_recipe = $(if $(call _file_in_depot,$1,$2),\
                                $(call _file_in_depot,$1,$2),\
                                $(call _file_in_recipe,$1,$2))

api_file        = $(call _file_in_depot_or_recipe,$1,api)
used_apis_file  = $(call _file_in_depot_or_recipe,$1,used_apis)

_pkg_archives_file = $(call recipe_dir,pkg/$(call archive_recipe,$1))/archives
_user_pkg_archives = $(patsubst _/%,$(call archive_user,$1)/%,\
                        $(call grep_archive_user,_,\
                           $(call file_content,$(call _pkg_archives_file,$1))))

pkg_src_archives = $(call grep_archive_type,src,$(call _user_pkg_archives,$1))
pkg_raw_archives = $(call grep_archive_type,raw,$(call _user_pkg_archives,$1))
pkg_pkg_archives = $(call grep_archive_type,pkg,$(call _user_pkg_archives,$1))
