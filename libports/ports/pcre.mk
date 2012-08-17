include ports/pcre.inc

PCRE_TBZ = $(PCRE).tar.bz2
PCRE_URL = ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/$(PCRE_TBZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(PCRE)

prepare-pcre: $(CONTRIB_DIR)/$(PCRE) include/pcre

$(CONTRIB_DIR)/$(PCRE): clean-pcre

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(PCRE_TBZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(PCRE_URL) && touch $@

$(CONTRIB_DIR)/$(PCRE): $(DOWNLOAD_DIR)/$(PCRE_TBZ)
	$(VERBOSE)tar xfj $< -C $(CONTRIB_DIR) && touch $@

include/pcre:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -sf ../../src/lib/pcre/include/pcre.h include/pcre

clean-pcre:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(PCRE)
	$(VERBOSE)rm -rf include/pcre
