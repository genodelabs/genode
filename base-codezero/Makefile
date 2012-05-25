#
# \brief  Download and prepare the Codezero kernel
# \author Norman Feske
# \date   2011-08-05
#

VERBOSE    ?= @
ECHO        = @echo
GIT_URL     = git://git.l4dev.org/codezero.git
GIT_REV     = 6fa4884a5a1cf6207372f69ae01e5faa6d5a39c8
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
	$(ECHO) "Check out upstream source code of the Codezero kernel"
	$(ECHO)
	$(ECHO) "The source code will be located at the '$(CONTRIB_DIR)/' directory."
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

