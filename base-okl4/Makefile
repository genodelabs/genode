#
# \brief  Download, unpack and patch OKL4 source code
# \author Stefan Kalkowski
# \date   2011-05-02

DOWNLOAD_DIR  = download
CONTRIB_DIR   = contrib/okl4

VERBOSE      ?= @
ECHO          = @echo
OKL4_VERSION  = okl4_2.1.1-patch.9
OKL4_ARCHIVE  = $(OKL4_VERSION).tar.gz
OKL4_URI      = http://wiki.ok-labs.com/downloads/release-2.1.1-patch.9/$(OKL4_ARCHIVE)
PATCHES       = $(shell find patches -name *.patch)

#
# Print help information by default
#
help:
	$(ECHO)
	$(ECHO) "Prepare the OKL4 base repository"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare  - download and extract the OKL4 source code"
	$(ECHO) "clean    - clean everything except downloaded archives"
	$(ECHO) "cleanall - clean everything including downloaded archives"
	$(ECHO)

$(DOWNLOAD_DIR)/$(OKL4_ARCHIVE):
	$(ECHO) "downloading source code to '$(DOWNLOAD_DIR)/'"
	$(VERBOSE)mkdir -p $(DOWNLOAD_DIR)
	$(VERBOSE)wget -c $(OKL4_URI) -O $@

$(CONTRIB_DIR): clean

$(CONTRIB_DIR): $(DOWNLOAD_DIR)/$(OKL4_ARCHIVE)
	$(ECHO) "unpacking source code to '$(CONTRIB_DIR)/'"
	$(VERBOSE)tar xzf $<
	$(VERBOSE)mv $(OKL4_VERSION) $@
	$(ECHO) "applying patches to '$(CONTRIB_DIR)/'"
	$(VERBOSE)for i in $(PATCHES); do patch -d $@ -p1 < $$i; done

prepare: $(CONTRIB_DIR)

clean:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)

cleanall: clean
	$(VERBOSE)rm -rf $(DOWNLOAD_DIR)
