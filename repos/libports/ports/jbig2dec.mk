JBIG2DEC = jbig2dec-0.11
GIT_URL  = git://git.ghostscript.com/jbig2dec.git
GIT_REV  = 58b513e3ec60feac13ea429c4aff12ea8a8de91d

#
# Check for tools
#
$(call check_tool,git)

#
# Interface to top-level prepare Makefile
#
PORTS += $(JBIG2DEC)

prepare-jbig2dec: $(CONTRIB_DIR)/$(JBIG2DEC) include/jbig2dec/jbig2.h

$(CONTRIB_DIR)/$(JBIG2DEC): clean-jbig2dec

#
# Port-specific local rules
#

$(DOWNLOAD_DIR)/$(JBIG2DEC)/.git:
	$(VERBOSE)git clone $(GIT_URL) $(DOWNLOAD_DIR)/$(JBIG2DEC) && \
	cd $(DOWNLOAD_DIR)/$(JBIG2DEC) && \
	git reset --hard $(GIT_REV) && \
	cd ../.. && touch $@
	
$(CONTRIB_DIR)/$(JBIG2DEC)/.git: $(DOWNLOAD_DIR)/$(JBIG2DEC)/.git
	$(VERBOSE)git clone $(DOWNLOAD_DIR)/$(JBIG2DEC) $(CONTRIB_DIR)/$(JBIG2DEC)
	
$(CONTRIB_DIR)/$(JBIG2DEC): $(CONTRIB_DIR)/$(JBIG2DEC)/.git

include/jbig2dec/jbig2.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s ../../$(CONTRIB_DIR)/$(JBIG2DEC)/jbig2.h $@

clean-jbig2dec:
	$(VERBOSE)rm -rf include/jbig2dec
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(JBIG2DEC)
