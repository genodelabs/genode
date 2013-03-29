include ports/fribidi.inc

FRIBIDI_TBZ2 = $(FRIBIDI).tar.bz2
FRIBIDI_URL  = http://fribidi.org/download/$(FRIBIDI_TBZ2)

#
# Interface to top-level prepare Makefile
#
PORTS += $(FRIBIDI)

prepare-fribidi: $(CONTRIB_DIR)/$(FRIBIDI) include/fribidi/fribidi.h

$(CONTRIB_DIR)/$(FRIBIDI):clean-fribidi

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(FRIBIDI_TBZ2):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(FRIBIDI_URL) && touch $@

$(CONTRIB_DIR)/$(FRIBIDI): $(DOWNLOAD_DIR)/$(FRIBIDI_TBZ2)
	$(VERBOSE)tar xfj $< -C $(CONTRIB_DIR) && touch $@

FRIBIDI_INCLUDES = fribidi.h

include/fribidi/fribidi.h:
	$(VERBOSE)for i in lib charset; do \
	  for j in `find $(CONTRIB_DIR)/$(FRIBIDI)/$$i -name "fribidi*.h"`; do \
	    name=`basename $$j`; \
	    ln -sf ../../$(CONTRIB_DIR)/$(FRIBIDI)/$$i/$$name $(dir $@)$$name; \
	  done; \
	done

clean-fribidi:
	$(VERBOSE)find include/fribidi -type l -delete
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(FRIBIDI)
