FREETYPE     = freetype-2.3.9
FREETYPE_TGZ = $(FREETYPE).tar.gz
FREETYPE_URL = http://mirrors.zerg.biz/nongnu/freetype/freetype-old/$(FREETYPE_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(FREETYPE)

prepare-freetype: $(CONTRIB_DIR)/$(FREETYPE) include/freetype include/ft2build.h

$(CONTRIB_DIR)/$(FREETYPE): clean-freetype

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(FREETYPE_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(FREETYPE_URL) && touch $@

$(CONTRIB_DIR)/$(FREETYPE): $(DOWNLOAD_DIR)/$(FREETYPE_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

include/freetype:
	$(VERBOSE)ln -s ../$(CONTRIB_DIR)/$(FREETYPE)/include/freetype $@

include/ft2build.h:
	$(VERBOSE)ln -s ../$(CONTRIB_DIR)/$(FREETYPE)/$@ $@

clean-freetype:
	$(VERBOSE)rm -rf include/freetype
	$(VERBOSE)rm -f  include/ft2build.h
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(FREETYPE)
