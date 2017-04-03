NANO      = nano-2.2.6
NANO_TGZ  = $(NANO).tar.gz
NANO_URL  = ftp://ftp.gnu.org/pub/gnu/nano
NANO_SIG  = $(NANO_TGZ).sig
NANO_KEY  = GNU
#
# Interface to top-level prepare Makefile
#
PORTS += $(NANO)

#

prepare:: $(CONTRIB_DIR)/$(NANO)

$(DOWNLOAD_DIR)/$(NANO_TGZ):
		$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(NANO_URL)/$(NANO_TGZ) && touch $@

$(DOWNLOAD_DIR)/$(NANO_SIG):
		$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(NANO_URL)/$(NANO_SIG) && touch $@

$(CONTRIB_DIR)/$(NANO): $(DOWNLOAD_DIR)/$(NANO_TGZ).verified
		$(VERBOSE)tar xzf $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

$(DOWNLOAD_DIR)/$(NANO_TGZ).verified: $(DOWNLOAD_DIR)/$(NANO_TGZ) \
		$(DOWNLOAD_DIR)/$(NANO_SIG)
		$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(NANO_TGZ) \
								 $(DOWNLOAD_DIR)/$(NANO_SIG) $(NANO_KEY)
		$(VERBOSE)touch $@
