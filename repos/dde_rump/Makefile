#
# \brief Checkout rump kernel source code
# \author Sebastian Sumpf
# \date   2013-11-27
#

VERBOSE    ?= @
CONTRIB_DIR = contrib
ECHO        = @echo
PATCHES    := $(shell find patches -name \*.patch)

GIT_RUMP_URI      = https://github.com/anttikantee/buildrump.sh.git
GIT_RUMP_COMMIT   = d604845baafa110945cb54a2d9607e6f435c6027

GIT_LIBS_URI      = https://github.com/anttikantee/xen-nblibc.git
GIT_LIBS_COMMIT   = b86122315f338042d06ad83ac5bd763a5dbd0c00

GIT_NETBSD_COMMIT = ff70642c9d7a8fb4b3242b30de2efc1fd8ad9ad4

RUMP_SRC   = src

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,git)

$(CONTRIB_DIR)/.git:
	$(VERBOSE) git clone $(GIT_RUMP_URI) $(CONTRIB_DIR)

$(CONTRIB_DIR)/nblibs:
	$(VERBOSE) git clone $(GIT_LIBS_URI) $(CONTRIB_DIR)/nblibs
	cd $(CONTRIB_DIR)/nblibs ; ln -sf ../$(RUMP_SRC)/common

$(CONTRIB_DIR)/$(RUMP_SRC):
	$(VERBOSE) cd $(CONTRIB_DIR); ./buildrump.sh -s $(RUMP_SRC) checkout
	$(VERBOSE) cd $(CONTRIB_DIR)/src; git reset --hard $(GIT_NETBSD_COMMIT)

commit:
	$(VERBOSE) cd $(CONTRIB_DIR); git fetch origin
	$(VERBOSE) cd $(CONTRIB_DIR); git reset --hard $(GIT_RUMP_COMMIT)
	$(VERBOSE) cd $(CONTRIB_DIR)/nblibs; git fetch origin
	$(VERBOSE) cd $(CONTRIB_DIR)/nblibs; git reset --hard $(GIT_LIBS_COMMIT)
	$(ECHO) "applying patches to '$(CONTRIB_DIR)/'"
	$(VERBOSE)for i in $(PATCHES); do patch -N -d $(CONTRIB_DIR) -p1 < $$i; done

prepare: $(CONTRIB_DIR)/.git $(CONTRIB_DIR)/nblibs $(CONTRIB_DIR)/$(RUMP_SRC) commit
	$(VERBOSE)mkdir -p include
	$(VERBOSE)ln -sf ../$(CONTRIB_DIR)/$(RUMP_SRC)/sys/rump/include/rump include/rump


clean:
	$(VERBOSE) rm -rf $(CONTRIB_DIR)

help::
	$(ECHO)
	$(ECHO) "Check out rump kernel sources"
	$(ECHO)
	$(ECHO) "The source code will be located at the '$(CONTRIB_DIR)/' directory."
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare                 - checkout source codes"
	$(ECHO) "clean                   - remove source codes"
	$(ECHO)

.NOTPARALLEL:
