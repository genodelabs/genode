GNUGZIP         = gzip-1.4
GNUGZIP_TGZ     = $(GNUGZIP).tar.gz
GNUGZIP_URL     = ftp://ftp.gnu.org/pub/gnu/gzip/$(GNUGZIP_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(GNUGZIP)

prepare:: $(CONTRIB_DIR)/$(GNUGZIP)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(GNUGZIP_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GNUGZIP_URL) && touch $@

$(CONTRIB_DIR)/$(GNUGZIP): $(DOWNLOAD_DIR)/$(GNUGZIP_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

