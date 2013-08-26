#
# \brief  Download integrate Linux kernel sources with Genode
# \author Norman Feske
# \date   2012-01-28

CONTRIB_DIR  = contrib
DOWNLOAD_DIR = download
VERBOSE     ?= @
ECHO         = @echo
PATCHES      := $(shell find patches -name \*.patch)

LINUX      = linux-3.9
LINUX_TGZ = $(LINUX).tar.gz
LINUX_URL  = http://www.kernel.org/pub/linux/kernel/v3.x/$(LINUX_TGZ)

# Raspberry Pi
DWC_OTG_GIT_URL    := https://github.com/nfeske/dwc_otg.git
DWC_OTG_GIT_BRANCH := r1

#
#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,wget)
$(call check_tool,patch)

#
# Print help information by default
#
help:
	$(ECHO)
	$(ECHO) "Download integrate Linux kernel sources with Genode"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare  - download and integrate Linux source code"
	$(ECHO) "clean    - remove contib sources except downloaded archives"
	$(ECHO) "cleanall - remove contib sources and downloaded archives"
	$(ECHO)

prepare: clean $(CONTRIB_DIR)/.prepared

prepare_rpi: prepare
	$(VERBOSE)cd $(CONTRIB_DIR)/drivers/usb/host; \
	    git clone $(DWC_OTG_GIT_URL) dwc_otg
	$(VERBOSE)cd $(CONTRIB_DIR)/drivers/usb/host/dwc_otg; \
	    git reset --hard HEAD && git checkout $(DWC_OTG_GIT_BRANCH)

$(CONTRIB_DIR)/.prepared: Makefile
$(CONTRIB_DIR)/.prepared: $(DOWNLOAD_DIR)/$(LINUX_TGZ)
	$(ECHO) "extracting source code to '$(CONTRIB_DIR)'"
	$(VERBOSE)tar xfz $< --transform "s{$(LINUX){$(CONTRIB_DIR){" --files-from files.list
	$(VERBOSE)tar xfz $< --transform "s{$(LINUX){$(CONTRIB_DIR)/lxip{" --files-from lxip_header.list
	$(VERBOSE)touch $@
	$(ECHO) "applying patches to '$(CONTRIB_DIR)/'"
	$(VERBOSE)for i in $(PATCHES); do patch -d $(CONTRIB_DIR) -p1 < $$i; done
	$(VERBOSE)touch $(CONTRIB_DIR)/drivers/usb/dwc3/gadget.h
	$(VERBOSE)touch $(CONTRIB_DIR)/drivers/usb/dwc3/debug.h


$(DOWNLOAD_DIR):
	$(VERBOSE)mkdir -p $@

$(DOWNLOAD_DIR)/$(LINUX_TGZ): $(DOWNLOAD_DIR)
	$(ECHO) "downloading source code to '$@'"
	$(VERBOSE)cd $(DOWNLOAD_DIR); wget -c $(LINUX_URL)
	$(VERBOSE)touch $@

clean:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)

cleanall: clean
	$(VERBOSE)rm -rf $(DOWNLOAD_DIR)
