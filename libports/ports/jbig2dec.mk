JBIG2DEC     = jbig2dec-0.11
JBIG2DEC_TGZ = $(JBIG2DEC).tar.gz
JBIG2DEC_URL = http://ghostscript.com/~giles/jbig2/jbig2dec/$(JBIG2DEC_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(JBIG2DEC)

prepare-jbig2dec: $(CONTRIB_DIR)/$(JBIG2DEC) include/jbig2dec/jbig2.h

$(CONTRIB_DIR)/$(JBIG2DEC): clean-jbig2dec

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(JBIG2DEC_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(JBIG2DEC_URL) && touch $@

$(CONTRIB_DIR)/$(JBIG2DEC): $(DOWNLOAD_DIR)/$(JBIG2DEC_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

include/jbig2dec/jbig2.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s ../../$(CONTRIB_DIR)/$(JBIG2DEC)/jbig2.h $@

clean-jbig2dec:
	$(VERBOSE)rm -rf include/jbig2dec
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(JBIG2DEC)
