#
# \brief  Download, unpack and patch OKLinux source code
# \author Stefan Kalkowski
# \date   2010-01-06

DOWNLOAD_DIR  = download
CONTRIB_DIR   = contrib
PATCH_FILE    = patches/oklx_genode.patch
PATCH_UNIONFS = patches/unionfs.patch

VERBOSE     ?= @
ECHO         = @echo
OKLX_URL     = http://wiki.ok-labs.com/downloads/release-2.1.1-patch.9
OKLX_VERSION = oklinux_2.6.23.24
OKLX_ARCHIVE = $(OKLX_VERSION).tar.gz
OKLX_CONTRIB = kernel-2.6.23-v2

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,wget)
$(call check_tool,patch)
$(call check_tool,sed)

#
# Print help information by default
#
help:
	$(ECHO)
	$(ECHO) "Prepare the OKLinux repository"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare      - download and extract the OKLinux source code"
	$(ECHO) "clean        - clean everything except downloaded archives"
	$(ECHO) "cleanall     - clean everything including downloaded archives"
	$(ECHO) "update-patch - updates patch to the original OKLinux code"
	$(ECHO)

prepare: $(CONTRIB_DIR) message

$(CONTRIB_DIR): $(DOWNLOAD_DIR)/$(OKLX_ARCHIVE)
	$(ECHO) "unpacking source code to '$(CONTRIB_DIR)/'"
	$(VERBOSE)tar xzf $<
	$(VERBOSE)mv $(OKLX_VERSION)/$(OKLX_CONTRIB) $@
	$(VERBOSE)rm -rf $(OKLX_VERSION)
	$(ECHO) "applying patches to '$(CONTRIB_DIR)/'"
	$(VERBOSE)patch -p0 < $(PATCH_FILE)
	$(VERBOSE)cd $(CONTRIB_DIR); patch --no-backup-if-mismatch -p1 < ../$(PATCH_UNIONFS)

$(DOWNLOAD_DIR)/$(OKLX_ARCHIVE):
	$(ECHO) "downloading source code to '$(DOWNLOAD_DIR)/'"
	$(VERBOSE)mkdir -p $(DOWNLOAD_DIR)
	$(VERBOSE)wget -c $(OKLX_URL)/$(OKLX_ARCHIVE) -O $@

message:
	$(ECHO)
	$(ECHO) "Preparation completed!"
	$(ECHO) "Now, go to your Genode build directory and type 'make oklinux'."
	$(ECHO) "Hint: don't forget to put '$(shell pwd)' "
	$(ECHO) "      as a repository into your build.conf"
	$(ECHO)

update-patch:
	$(ECHO) "producing a new diff and save it to '$(PATCH_FILE)'"
	$(VERBOSE)cd $(CONTRIB_DIR); patch --no-backup-if-mismatch -R -p1 < ../$(PATCH_UNIONFS)
	$(VERBOSE)tar xzf $(DOWNLOAD_DIR)/$(OKLX_ARCHIVE)
	$(VERBOSE)mv $(OKLX_VERSION)/$(OKLX_CONTRIB) .
	$(VERBOSE)find $(CONTRIB_DIR) -name "*~" -delete
	$(VERBOSE)LC_COLLATE=C diff -urNpB $(OKLX_CONTRIB) $(CONTRIB_DIR) \
	          | sed "s/\(^--- [^\t]*\).*/\\1/" \
	          | sed "s/\(^+++ [^\t]*\).*/\\1/" \
	          > $(PATCH_FILE) || true
	$(VERBOSE)rm -rf $(OKLX_CONTRIB) $(OKLX_VERSION)
	$(VERBOSE)cd $(CONTRIB_DIR); patch --no-backup-if-mismatch -p1 < ../$(PATCH_UNIONFS)

clean:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)

cleanall: clean
	$(VERBOSE)rm -rf $(DOWNLOAD_DIR)

