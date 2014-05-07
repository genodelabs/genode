include ports/fribidi.inc

FRIBIDI_TBZ2     = $(FRIBIDI).tar.bz2
FRIBIDI_SHA      = $(FRIBIDI_TBZ2).sha256
FRIBIDI_SHA_SIG  = $(FRIBIDI_TBZ2).sha256.asc
FRIBIDI_BASE_URL = http://fribidi.org/download
FRIBIDI_URL      = $(FRIBIDI_BASE_URL)/$(FRIBIDI_TBZ2)
FRIBIDI_URL_SHA  = $(FRIBIDI_BASE_URL)/$(FRIBIDI_SHA)
FRIBIDI_URL_SIG  = $(FRIBIDI_BASE_URL)/$(FRIBIDI_SHA_SIG)
FRIBIDI_KEY      = D3531115

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

$(DOWNLOAD_DIR)/$(FRIBIDI_SHA):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(FRIBIDI_URL_SHA) && touch $@

$(DOWNLOAD_DIR)/$(FRIBIDI_SHA_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(FRIBIDI_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(FRIBIDI_TBZ2).verified: $(DOWNLOAD_DIR)/$(FRIBIDI_TBZ2) \
                                          $(DOWNLOAD_DIR)/$(FRIBIDI_SHA) \
                                          $(DOWNLOAD_DIR)/$(FRIBIDI_SHA_SIG)
	# XXX fribidi does NOT create a detached signature and thus the signature
	# checking is useless !!! -- somebody should inform them
	# see http://blog.terryburton.co.uk/2006/11/falling-into-trap-with-gpg.html
	#$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(FRIBIDI_SHA) $(DOWNLOAD_DIR)/$(FRIBIDI_SHA_SIG) $(FRIBIDI_KEY)
	$(VERBOSE)$(HASHVERIFIER) $(DOWNLOAD_DIR)/$(FRIBIDI_TBZ2) $(DOWNLOAD_DIR)/$(FRIBIDI_SHA) sha256
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(FRIBIDI): $(DOWNLOAD_DIR)/$(FRIBIDI_TBZ2).verified
	$(VERBOSE)tar xfj $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

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
