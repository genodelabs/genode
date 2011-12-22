JPEG     = jpeg-7
JPEG_TGZ = jpegsrc.v7.tar.gz
JPEG_URL = http://www.ijg.org/files/$(JPEG_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(JPEG)

prepare-jpeg: $(CONTRIB_DIR)/$(JPEG)

$(CONTRIB_DIR)/$(JPEG): clean-jpeg

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(JPEG_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(JPEG_URL) && touch $@

$(CONTRIB_DIR)/$(JPEG): $(DOWNLOAD_DIR)/$(JPEG_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

clean-jpeg:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(JPEG)
