#
# \brief  Download and patch port of 3rd-party source code
# \author Norman Feske
# \date   2014-05-07
#
# This makefile must be invoked from the port directory.
#
# Arguments:
#
# PORT - port description file
#
# This makefile includes the port description file. The namespace for variables
# is organized as follows: Variables and functions that are private to the
# prepare_port tool are prefixed with '_'. Port description files must not
# declare any variable with a leading '_'. Variables that interface the
# prepare_port tool with the port description file are written uppercase. Local
# helper variables in the port description files should be written lowercase.
#

# XXX remove this line when the tool has stabilized
STRICT_HASH ?= no

#
# Utility to check if a python module is installed
#
check_python_module = $(if $(shell python -c "import $(1)" 2>&1),$(error Need to have python module '$(1)' installed.),)

default:

.NOTPARALLEL: default

# repository that contains the port description, used to look up patch files
REP_DIR := $(realpath $(dir $(PORT))/..)

#
# Check presence of argument $1. Back out with error message $2 if not defined.
#
_assert = $(if $(strip $1),$1,$(info Error: $(strip $2))$(error ))

#
# Helper function that returns $1 if defined, otherwise it returns $2
#
_prefer = $(if $1,$1,$2)

#
# Include common definitions
#
include $(GENODE_DIR)/tool/ports/mk/common.inc

#
# Include definitions provided by the port description file
#
include $(PORT)

$(call check_tool,wget)
$(call check_tool,patch)
$(call check_tool,sha256sum)

#
# Assertion for the presence of a LICENSE and VERSION declarations in the port
# description
#
ifeq ($(LICENSE),)
default: license_undefined
license_undefined:
	@$(ECHO) "Error: License undefined"; false
endif

ifeq ($(VERSION),)
default: version_undefined
version_undefined:
	@$(ECHO) "Error: Version undefined"; false
endif

_DST_HASH_FILE := $(notdir $(PORT:.port=)).hash


#
# Default rule that triggers the actual preparation steps
#
default: $(DOWNLOADS) _patch $(_DST_HASH_FILE) _dirs

_dirs: $(DOWNLOADS)


##
## Generate the HASH file
##

include $(GENODE_DIR)/tool/ports/mk/hash.inc


##
## Apply patches
##

# default arguments for the patch command
PATCH_OPT ?= -p0

#
# Helper function to obtain options for applying a patch
#
_patch_opt = $(call _prefer,$(PATCH_OPT($1)),$(PATCH_OPT))

#
# Helper function to look up the input file for a patch
#
_patch_input = $(wildcard $1 $(REP_DIR)/$1)

#
# Rules for applying patches
#
# The 'patch' rule is a phony rule that is used as default rule. It triggers
# all other steps such as downloading and hash-sum checks. For each patch, there
# dependency to a phony rule.
#
_PATCH_TARGETS := $(addprefix phony/patches/,$(PATCHES))

_patch: $(_PATCH_TARGETS)

$(_PATCH_TARGETS): $(DOWNLOADS)

phony/patches/%:
	@$(MSG_APPLY)$*
	$(VERBOSE)test -f $(firstword $(call _patch_input,$*) fail) ||\
		($(ECHO) "Error: Could not find patch $*"; false)
	$(VERBOSE)for i in $(call _patch_input,$*); do\
	              patch -s $(call _patch_opt,$*) < $$i; done


##
## Assemble custom directory structures within the port directory
##

_DIR_TARGETS := $(addprefix phony/dirs/,$(DIRS))

_dirs: $(_DIR_TARGETS)

$(_DIR_TARGETS): _patch

_dir_content = $(call _assert,$(DIR_CONTENT($1)), \
                              Missing declaration of DIR_CONTENT($1))

phony/dirs/%:
	@$(MSG_INSTALL)$*
	$(VERBOSE)mkdir -p $*
	$(VERBOSE)cp -Lrf $(call _dir_content,$*) $*


##
## Obtain source codes from a Git repository
##

_git_dir = $(call _assert,$(DIR($1)),Missing declaration of DIR($*))

%.git:
	$(VERBOSE)test -n "$(REV($*))" ||\
		($(ECHO) "Error: Undefined revision for $*"; false);
	$(VERBOSE)test -n "$(URL($*))" ||\
		($(ECHO) "Error: Undefined URL for $*"; false);
	$(VERBOSE)dir=$(call _git_dir,$*);\
		test -d $$dir || $(MSG_DOWNLOAD)$(URL($*)); \
		test -d $$dir || git clone $(URL($*)) $$dir &> >(sed 's/^/$(MSG_GIT)/'); \
		$(MSG_UPDATE)$$dir; \
		cd $$dir && $(GIT) fetch && $(GIT) reset -q --hard HEAD && $(GIT) checkout -q $(REV($*))


##
## Obtain source codes from Subversion repository
##

_svn_dir = $(call _assert,$(DIR($1)),Missing declaration of DIR($*))

%.svn:
	$(VERBOSE)test -n "$(REV($*))" ||\
		($(ECHO) "Error: Undefined revision for $*"; false);
	$(VERBOSE)test -n "$(URL($*))" ||\
		($(ECHO) "Error: Undefined URL for $*"; false);
	$(VERBOSE)dir=$(call _svn_dir,$*);\
		rm -rf $$dir; \
		$(MSG_DOWNLOAD)$(URL($*)); \
		svn export -q $(URL($*))@$(REV($*)) $$dir;


##
## Download a plain file
##

_file_name = $(call _prefer,$(NAME($1)),$(notdir $(URL($1))))

# Some downloads are available via HTTPS only, but wget < 3.14 does not support
# server-name identification, which is used by some sites. So, we disable
# certificate checking in wget and check the validity of the download via SIG
# or SHA.

%.file:
	$(VERBOSE)test -n "$(URL($*))" ||\
		($(ECHO) "Error: Undefined URL for $(call _file_name,$*)"; false);
	$(VERBOSE)mkdir -p $(dir $(call _file_name,$*))
	$(VERBOSE)name=$(call _file_name,$*);\
		(test -f $$name || $(MSG_DOWNLOAD)$(URL($*))); \
		(test -f $$name || wget --quiet --no-check-certificate $(URL($*)) -O $$name) || \
			($(ECHO) Error: Download for $* failed; false)
	$(VERBOSE)\
		($(ECHO) "$(SHA($*))  $(call _file_name,$*)" |\
		sha256sum -c > /dev/null 2> /dev/null) || \
			($(ECHO) Error: Hash sum check for $* failed; false)


##
## Download and unpack an archive
##

_archive_name = $(call _prefer,$(NAME($1)),$(notdir $(URL($1))))
_archive_dir  = $(call _assert,$(DIR($1)),Missing definition of DIR($*) in $(PORT))

_tar_opt   = $(call _prefer,$(TAR_OPT($1)),--strip-components=1)
_unzip_opt = $(call _prefer,$(UNZIP_OPT($1)),$(UNZIP_OPT))

#
# Archive extraction functions for various archive types
#
_extract_function(tar)     = tar xf  $(ARCHIVE) -C $(DIR) $(call _tar_opt,$1)
_extract_function(tgz)     = tar xfz $(ARCHIVE) -C $(DIR) $(call _tar_opt,$1)
_extract_function(tar.gz)  = tar xfz $(ARCHIVE) -C $(DIR) $(call _tar_opt,$1)
_extract_function(tar.xz)  = tar xfJ $(ARCHIVE) -C $(DIR) $(call _tar_opt,$1)
_extract_function(tar.bz2) = tar xfj $(ARCHIVE) -C $(DIR) $(call _tar_opt,$1)
_extract_function(txz)     = tar xfJ $(ARCHIVE) -C $(DIR) $(call _tar_opt,$1)
_extract_function(zip)     = unzip -o -q -d $(DIR) $(call _unzip_opt,$1) $(ARCHIVE)

_ARCHIVE_EXTS := tar tar.gz tar.xz tgz tar.bz2 txz zip

#
# Function that returns the matching extraction function for a given archive
#
# Because this function refers to the 'ARCHIVE' variable, it is only supposed
# to work from the scope of the %.archive rule.
#
_extract_function = $(call _assert,\
                           $(foreach E,$(_ARCHIVE_EXTS),\
                                     $(if $(filter %.$E,$(ARCHIVE)),\
                                          $(_extract_function($E)),)),\
                           Don't know how to extract $(ARCHIVE))

%.archive: ARCHIVE = $(call _archive_name,$*)
%.archive: DIR     = $(call _archive_dir,$*)

%.archive: %.file
	@$(MSG_EXTRACT)"$(ARCHIVE) ($*)"
	$(VERBOSE)\
		mkdir -p $(DIR);\
		$(call _extract_function,$*)
