#
# \brief  Fetch and patch iPXE source code
# \author Stefan Kalkowski
# \author Christian Helmuth
# \date   2011-08-12
#

VERBOSE     ?= @
ECHO         = @echo
GIT_URL      = git://git.ipxe.org/ipxe.git
GIT_REV      = 174df77359f22f3be2169e9bb04e8018015b5e94
CONTRIB_DIR  = contrib
PATCH_FILE   = patches/dde_ipxe.patch

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,git)
$(call check_tool,patch)

#
# Print help information by default
#
help:
	$(ECHO)
	$(ECHO) "Prepare the dde_ipxe repository"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare      - fetch and patch iPXE source code"
	$(ECHO) "clean        - revert patch from iPXE souce code"
	$(ECHO) "cleanall     - remove iPXE souce code"
	$(ECHO) "update-patch - updates patch for iPXE source code"
	$(ECHO)

$(CONTRIB_DIR)/.git:
	$(VERBOSE)git clone $(GIT_URL) $(CONTRIB_DIR)

prepare: $(CONTRIB_DIR)/.git clean
	$(ECHO) "apply patch to '$(CONTRIB_DIR)/'"
	$(VERBOSE)patch -p1 -d $(CONTRIB_DIR) -i $(realpath $(PATCH_FILE))
	$(ECHO)
	$(ECHO) "Preparation completed!"
	$(ECHO) "Hint: don't forget to put '$(shell pwd)' "
	$(ECHO) "      as a repository into your build.conf"
	$(ECHO)

update-patch:
	$(ECHO) "producing a new diff and save it to '$(PATCH_FILE)'"
	$(VERBOSE)(cd $(CONTRIB_DIR); LC_COLLATE=C git diff) > $(PATCH_FILE) || true
#	$(VERBOSE)(cd $(CONTRIB_DIR); LC_COLLATE=C git diff) \
#	          | sed "s/\(^--- [^\t]*\).*/\\1/" \
#	          | sed "s/\(^+++ [^\t]*\).*/\\1/" \
#	          > $(PATCH_FILE) || true

clean:
	$(VERBOSE)cd $(CONTRIB_DIR); git reset --hard $(GIT_REV)
	$(VERBOSE)cd $(CONTRIB_DIR); git ls-files -o | xargs rm -rf

cleanall:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)
