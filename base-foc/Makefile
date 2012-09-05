#
# \brief  Checkout Fiasco.OC and addtional needed tools (sigma0, bootstrap)
# \author Stefan Kalkowski
# \date   2011-03-31
#

VERBOSE    ?= @
ECHO        = @echo
SVN_URI     = http://svn.tudos.org/repos/oc/tudos/trunk
SVN_REV     = 40
CONTRIB_DIR = contrib
PATCHES     = $(shell find patches -name *.patch)

SVN_TARGETS = tools/preprocess \
              kernel/fiasco \
              l4/conf \
              l4/doc \
              l4/mk \
              l4/tool \
              l4/pkg/bootstrap \
              l4/pkg/cxx \
              l4/pkg/drivers-frst \
              l4/pkg/l4sys \
              l4/pkg/l4util \
              l4/pkg/ldscripts \
              l4/pkg/libgcc-pure \
              l4/pkg/libgcc \
              l4/pkg/libsigma0 \
              l4/pkg/sigma0 \
              l4/pkg/uclibc-headers \
              l4/pkg/uclibc-minimal \
              l4/pkg/uclibc \
              l4/pkg/libvcpu

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,patch)
$(call check_tool,svn)

#
# Print help information by default
#
help::

# realpath is there to follow symlink; if contrib dir does not exists yet,
# create new directory
REAL_CONTRIB_DIR := $(realpath $(CONTRIB_DIR))
ifeq ($(REAL_CONTRIB_DIR),)
REAL_CONTRIB_DIR := $(CONTRIB_DIR)
endif

prepare: $(REAL_CONTRIB_DIR)/.svn update_contrib_subdirs apply_patches

help::
	$(ECHO)
	$(ECHO) "Check out upstream source code of Fiasco.OC"
	$(ECHO)
	$(ECHO) "The source code will be located at the '$(CONTRIB_DIR)/' directory."
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare - checkout upstream source codes"
	$(ECHO) "clean   - remove upstream source codes"
	$(ECHO)

$(CONTRIB_DIR):
	$(VERBOSE)mkdir -p $@

# use '.svn' subdirectory as rule to enable the use of a symbolic link as
# contrib directory
$(REAL_CONTRIB_DIR)/.svn: $(CONTRIB_DIR)
	$(VERBOSE)svn co -r $(SVN_REV) --depth immediates $(SVN_URI) $(dir $@)
	$(VERBOSE)svn co -r $(SVN_REV) --depth files $(SVN_URI)/l4 $(dir $@)/l4
	$(VERBOSE)svn co -r $(SVN_REV) --depth files $(SVN_URI)/l4/pkg $(dir $@)/l4/pkg

# used phony to always update the SVN on 'make prepare'
# (before updating, we need to revert our custom patches)
update_contrib_subdirs: $(addprefix $(REAL_CONTRIB_DIR)/,$(SVN_TARGETS))
	$(ECHO) "updating . to revision $(SVN_REV)"
	$(VERBOSE)svn revert $(REAL_CONTRIB_DIR)
	$(VERBOSE)svn up -r $(SVN_REV) -N  $(REAL_CONTRIB_DIR)/
	$(ECHO) "updating l4 to revision $(SVN_REV)"
	$(VERBOSE)svn revert $(REAL_CONTRIB_DIR)/l4
	$(VERBOSE)svn up -r $(SVN_REV) -N  $(REAL_CONTRIB_DIR)/l4
	$(ECHO) "updating l4/pkg to revision $(SVN_REV)"
	$(VERBOSE)svn revert $(REAL_CONTRIB_DIR)/l4/pkg
	$(VERBOSE)svn up -r $(SVN_REV) -N  $(REAL_CONTRIB_DIR)/l4/pkg
	$(VERBOSE)for i in $(SVN_TARGETS); do \
		echo "updating $$i to revision $(SVN_REV)"; \
		svn revert -R $(REAL_CONTRIB_DIR)/$$i; \
		svn up -r $(SVN_REV) $(REAL_CONTRIB_DIR)/$$i; done

# for resolving the dependencies of 'update_contrib_subdirs'
$(REAL_CONTRIB_DIR)/%:
	$(VERBOSE)svn co -r $(SVN_REV) $(SVN_URI)/$* $@

apply_patches:
	$(ECHO) "applying patches to '$(REAL_CONTRIB_DIR)/'"
	$(VERBOSE)for i in $(PATCHES); do \
	            patch -N -d $(REAL_CONTRIB_DIR) -p0 < $$i; done

# if $(CONTRIB_DIR) is a symlink, leave $(REAL_CONTRIB_DIR) alone
clean::
	$(VERBOSE)rm -rf $(CONTRIB_DIR)

.NOTPARALLEL:
