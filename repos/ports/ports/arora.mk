ARORA     = arora-0.11.0
ARORA_TGZ = $(ARORA).tar.gz
ARORA_URL = http://arora.googlecode.com/files/$(ARORA_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(ARORA)

prepare-arora: $(CONTRIB_DIR)/$(ARORA)

PATCHES_DIR = src/app/arora/patches

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(ARORA_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(ARORA_URL) && touch $@

$(CONTRIB_DIR)/$(ARORA): $(DOWNLOAD_DIR)/$(ARORA_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)for p in $(shell cat $(PATCHES_DIR)/series); do \
		patch -d $@ -p1 -i ../../$(PATCHES_DIR)/$$p; done;

clean-arora:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(ARORA)
