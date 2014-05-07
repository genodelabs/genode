FREETYPE          = freetype-2.3.9
FREETYPE_TGZ      = $(FREETYPE).tar.gz
FREETYPE_SIG      = $(FREETYPE_TGZ).sig
FREETYPE_BASE_URL = http://mirrors.zerg.biz/nongnu/freetype/freetype-old
FREETYPE_URL      = $(FREETYPE_BASE_URL)/$(FREETYPE_TGZ)
FREETYPE_URL_SIG  = $(FREETYPE_BASE_URL)/$(FREETYPE_SIG)
FREETYPE_KEY      = wl@gnu.org

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

$(DOWNLOAD_DIR)/$(FREETYPE_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(FREETYPE_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(FREETYPE_TGZ).verified: $(DOWNLOAD_DIR)/$(FREETYPE_TGZ) $(DOWNLOAD_DIR)/$(FREETYPE_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(FREETYPE_TGZ) $(DOWNLOAD_DIR)/$(FREETYPE_SIG) $(FREETYPE_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(FREETYPE): $(DOWNLOAD_DIR)/$(FREETYPE_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

include/freetype:
	$(VERBOSE)ln -s ../$(CONTRIB_DIR)/$(FREETYPE)/include/freetype $@

include/ft2build.h:
	$(VERBOSE)ln -s ../$(CONTRIB_DIR)/$(FREETYPE)/$@ $@

clean-freetype:
	$(VERBOSE)rm -rf include/freetype
	$(VERBOSE)rm -f  include/ft2build.h
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(FREETYPE)
