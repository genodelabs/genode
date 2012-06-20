#
# \brief  Download, and unpack the NOVA hypervisor.
# \author Stefan Kalkowski
# \author Alexander Boettcher
# \date   2012-06-04
#

VERBOSE     ?= @
ECHO        = @echo
GIT_URL     = git://github.com/IntelLabs/NOVA.git 
GIT_REV     = f6bad89f2df036c9ee75699c2138586e28c711a0 
CONTRIB_DIR = contrib
PATCHES     = $(shell find patches -name *.patch)

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

$(CONTRIB_DIR): $(CONTRIB_DIR)/.git
	$(VERBOSE)cd $(CONTRIB_DIR); git reset --hard $(GIT_REV)
	$(ECHO) "applying patches to '$(CONTRIB_DIR)/'"
	$(VERBOSE)for i in $(PATCHES); do patch -d $@ -p1 < $$i; done

.PHONY: $(CONTRIB_DIR)

clean::
	$(VERBOSE)rm -rf $(CONTRIB_DIR)
