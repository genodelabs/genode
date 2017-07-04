#
# \brief  Dependency-resolution functions
# \author Norman Feske
# \date   2017-03-22
#
# The following accessor functions must be defined when including this file:
#
#   api_file          return path to 'api' file of a library source archive
#   used_apis_file    return path to 'used_apis' file for a source archive
#   pkg_src_archives  return source archives of a package
#   pkg_raw_archives  return raw-data archives of a package
#   pkg_pkg_archives  return package-archive dependencies of a package
#

#
# Extend ARCHIVES(pkg) with package dependencies
#

_pkg_archives = $(call pkg_pkg_archives,$1)

_check_unvisited = $(if $(filter $(VISITED_PKGS),$1),\
                      $(error recursive package dependency: $(VISITED_PKGS)),\
                      $(eval VISITED_PKGS += $1))

_pkg_deps_rec = $(call _check_unvisited,$1) $1 \
                   $(foreach PKG,$(call _pkg_archives,$1),\
                      $(call _pkg_deps_rec,$(PKG)))

_pkg_deps = $(foreach PKG,$1,\
               $(eval VISITED_PKGS :=) $(call _pkg_deps_rec,$(PKG)))

ARCHIVES(pkg) := $(sort ${ARCHIVES(pkg)} $(call _pkg_deps,${ARCHIVES(pkg)}))


#
# Extend ARCHIVES(binpkg) with binary-package dependencies
#

# return list of source archives needed by a given binary package
_binpkg_src_archives = $(call pkg_src_archives,$(call pkg_of_binpkg,$1))

# return list of package archives contained in a binary package
_binpkg_pkg_archives = $(foreach P,$(call pkg_pkg_archives,$(call pkg_of_binpkg,$1)),\
                          $(call binpkg_for_pkg,$(call bin_archive_spec,$1),$P))

# override behavior of '_pkg_archives' to work on binary packages
_pkg_archives = $(call _binpkg_pkg_archives,$1)

ARCHIVES(binpkg) := $(sort ${ARCHIVES(binpkg)} $(call _pkg_deps,${ARCHIVES(binpkg)}))


#
# Extend ARCHIVES(pkg) with binary-package dependencies
#

ARCHIVES(pkg) := $(sort ${ARCHIVES(pkg)} $(foreach P,${ARCHIVES(binpkg)},\
                                            $(call pkg_of_binpkg,$P)))

#
# Extend ARCHIVES(bin) with binary-package dependencies
#

# return non-empty result if source archive exists
_src_exists = $(wildcard $(dir $(call used_apis_file,$1)))

# return binary-archive path for architecture $1 and source archive $2
_bin_for_src = $(strip \
                  $(if $(call _src_exists,$2),\
                     $(call archive_user,$2)/bin/$1/$(call archive_recipe,$2)))

# return list of binary archives contained in a binary package
_binpkg_bin_archives = $(foreach S,$(call _binpkg_src_archives,$1),\
                          $(call _bin_for_src,$(call bin_archive_spec,$1),$S))


ARCHIVES(bin) := $(sort ${ARCHIVES(bin)} $(foreach P,${ARCHIVES(binpkg)},\
                                            $(call _binpkg_bin_archives,$P)))

#
# Extend ARCHIVES(src) with package dependencies
#

ARCHIVES(src) := $(sort ${ARCHIVES(src)} $(foreach P,${ARCHIVES(pkg)},\
                                            $(call pkg_src_archives,$P)))

#
# Extend ARCHIVES(raw) with package dependencies
#

ARCHIVES(raw) := $(sort ${ARCHIVES(raw)} $(foreach P,${ARCHIVES(pkg)},\
                                            $(call pkg_raw_archives,$P)))

#
# Extend ARCHIVES(src) with binary dependencies
#

ARCHIVES(src) := $(sort ${ARCHIVES(src)}\
                    $(foreach B,${ARCHIVES(bin)},\
                       $(call src_of_bin,$B)))

#
# Extend ARCHIVES(api) with the APIs used by ARCHIVES(src)
#

_used_apis = $(call file_content,$(call used_apis_file,$1))

src_api_archives = $(addprefix $(call archive_user,$1)/api/,$(call _used_apis,$1))

ARCHIVES(api) := $(sort ${ARCHIVES(api)} $(foreach A,${ARCHIVES(src)},\
                                            $(call src_api_archives,$A)))

#
# Extend ARCHIVES(api) with the APIs implemented by library ARCHIVES(src)
#

_lib_api = $(call file_content,$(call api_file,$1))

_lib_api_archive = $(addprefix $(call archive_user,$1)/api/,$(call _lib_api,$1))

ARCHIVES(api) := $(sort ${ARCHIVES(api)} $(foreach A,${ARCHIVES(src)},\
                                            $(call _lib_api_archive,$A)))
