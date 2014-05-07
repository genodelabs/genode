#
# \brief  Download, and unpack the NOVA hypervisor.
# \author Stefan Kalkowski
# \author Alexander Boettcher
# \date   2012-06-04
#

VERBOSE     ?= @
ECHO        = @echo
GIT_URL     = https://github.com/alex-ab/NOVA.git
GIT_REV     = HEAD
GIT_BRANCH  = r5
CONTRIB_DIR = contrib
PATCHES     = $(shell find patches -name '*.patch')

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,git)
$(call check_tool,patch)

#
# Print help information by default
#
help::

prepare: $(CONTRIB_DIR)

help::
	$(ECHO)
	$(ECHO) "Prepare the NOVA base repository"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare - checkout upstream source codes"
	$(ECHO) "clean   - remove upstream source codes"
	$(ECHO)

$(CONTRIB_DIR)/.git:
	$(VERBOSE)git clone $(GIT_URL) $(CONTRIB_DIR)

.PHONY: $(CONTRIB_DIR)

$(CONTRIB_DIR): $(CONTRIB_DIR)/.git
	$(VERBOSE)cd $(CONTRIB_DIR); git pull
	$(VERBOSE)cd $(CONTRIB_DIR); git reset --hard $(GIT_REV); git checkout $(GIT_BRANCH)
	$(ECHO) "applying patches to '$(CONTRIB_DIR)/'"
	$(VERBOSE)for i in $(PATCHES); do patch -d $@ -p1 < $$i; done

.PHONY: $(CONTRIB_DIR)

clean::
	$(VERBOSE)rm -rf $(CONTRIB_DIR)
