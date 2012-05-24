include lib/import/import-av.inc

LIBAV_TGZ = $(LIBAV).tar.gz
LIBAV_URL = http://www.libav.org/releases/$(LIBAV_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(LIBAV)

prepare-libav: $(CONTRIB_DIR)/$(LIBAV)

$(CONTRIB_DIR)/$(LIBAV): clean-libav

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LIBAV_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIBAV_URL) && touch $@

$(CONTRIB_DIR)/$(LIBAV): $(DOWNLOAD_DIR)/$(LIBAV_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(LIBAV) -p1 -i ../../src/app/avplay/avplay.patch

clean-libav:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBAV)
