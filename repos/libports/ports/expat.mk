include lib/import/import-expat.mk

EXPAT_TGZ = $(EXPAT).tar.gz
EXPAT_URL = http://sourceforge.net/projects/expat/files/expat/$(EXPAT_VER)/$(EXPAT_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(EXPAT)

prepare-expat: $(CONTRIB_DIR)/$(EXPAT)

$(CONTRIB_DIR)/$(EXPAT):clean-expat

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(EXPAT_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(EXPAT_URL) && touch $@

$(CONTRIB_DIR)/$(EXPAT): $(DOWNLOAD_DIR)/$(EXPAT_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

clean-expat:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(EXPAT)
