#
# \brief  Download, and unpack Fiasco and addtional needed tools (sigma0, bootstrap)
# \author Stefan Kalkowski
# \date   2011-07-18
#

VERBOSE       ?= @
ECHO           = @echo
DOWNLOAD_DIR   = download
CONTRIB_DIR    = contrib
FIASCO_ARCHIVE = 3rd_fiasco.tar.bz2
FIASCO_URI     = http://downloads.sourceforge.net/project/genode/3rd/$(FIASCO_ARCHIVE)
PATCHES       = $(shell find patches -name *.patch)

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,wget)
$(call check_tool,patch)
$(call check_tool,tar)
$(call check_tool,byacc)

#
# Print help information by default
#
help:
	$(ECHO)
	$(ECHO) "Prepare the Fiasco base repository"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare  - download and extract the Fiasco source code"
	$(ECHO) "clean    - clean everything except downloaded archives"
	$(ECHO) "cleanall - clean everything including downloaded archives"
	$(ECHO)

$(DOWNLOAD_DIR)/$(FIASCO_ARCHIVE):
	$(ECHO) "downloading source code to '$(DOWNLOAD_DIR)/'"
	$(VERBOSE)mkdir -p $(DOWNLOAD_DIR)
	$(VERBOSE)wget -c $(FIASCO_URI) -O $@

$(CONTRIB_DIR): $(DOWNLOAD_DIR)/$(FIASCO_ARCHIVE)
	$(ECHO) "unpacking source code to '$(CONTRIB_DIR)/'"
	$(VERBOSE)tar xjf $<
	$(VERBOSE)mv 3rd $@
	$(ECHO) "applying patches to '$@/'"
	$(VERBOSE)for i in $(PATCHES); do patch -d $@ -p0 < $$i; done
	$(VERBOSE)touch $@

prepare: $(CONTRIB_DIR)

clean:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)

cleanall: clean
	$(VERBOSE)rm -rf $(DOWNLOAD_DIR)
