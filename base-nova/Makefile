#
# \brief  Download, and unpack the NOVA hypervisor.
# \author Stefan Kalkowski
# \date   2011-07-20
#

VERBOSE     ?= @
ECHO         = @echo
DOWNLOAD_DIR = download
CONTRIB_DIR  = contrib
NOVA_ARCHIVE = nova-hypervisor-0.4.tar.bz2
NOVA_URI     = http://os.inf.tu-dresden.de/~us15/nova/$(NOVA_ARCHIVE)

#
# Print help information by default
#
help:
	$(ECHO)
	$(ECHO) "Prepare the NOVA base repository"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare  - download and extract the NOVA source code"
	$(ECHO) "clean    - clean everything except downloaded archives"
	$(ECHO) "cleanall - clean everything including downloaded archives"
	$(ECHO)

$(DOWNLOAD_DIR)/$(NOVA_ARCHIVE):
	$(ECHO) "downloading source code to '$(DOWNLOAD_DIR)/'"
	$(VERBOSE)mkdir -p $(DOWNLOAD_DIR)
	$(VERBOSE)wget -c $(NOVA_URI) -O $@

$(CONTRIB_DIR): clean

$(CONTRIB_DIR): $(DOWNLOAD_DIR)/$(NOVA_ARCHIVE)
	$(ECHO) "unpacking source code to '$(CONTRIB_DIR)/'"
	$(VERBOSE)tar xjf $<
	$(VERBOSE)mv hypervisor $@
	$(VERBOSE)patch -d $@ -p1 < patches/utcb.patch
	$(VERBOSE)touch $@

prepare: $(CONTRIB_DIR)

clean:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)

cleanall: clean
	$(VERBOSE)rm -rf $(DOWNLOAD_DIR)
