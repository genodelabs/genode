GNUMAKE     = make-3.82
GNUMAKE_TGZ = $(GNUMAKE).tar.gz
GNUMAKE_URL = http://ftp.gnu.org/pub/gnu/make/$(GNUMAKE_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(GNUMAKE)

prepare:: $(CONTRIB_DIR)/$(GNUMAKE)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(GNUMAKE_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GNUMAKE_URL) && touch $@

$(CONTRIB_DIR)/$(GNUMAKE): $(DOWNLOAD_DIR)/$(GNUMAKE_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

