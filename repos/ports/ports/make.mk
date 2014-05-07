GNUMAKE          = make-3.82
GNUMAKE_TGZ      = $(GNUMAKE).tar.gz
GNUMAKE_SIG      = $(GNUMAKE_TGZ).sig
GNUMAKE_BASE_URL = http://ftp.gnu.org/pub/gnu/make
GNUMAKE_URL      = $(GNUMAKE_BASE_URL)/$(GNUMAKE_TGZ)
GNUMAKE_URL_SIG  = $(GNUMAKE_BASE_URL)/$(GNUMAKE_SIG)
GNUMAKE_KEY      = GNU

#
# Interface to top-level prepare Makefile
#
PORTS += $(GNUMAKE)

prepare-make: $(CONTRIB_DIR)/$(GNUMAKE)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(GNUMAKE_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GNUMAKE_URL) && touch $@

$(DOWNLOAD_DIR)/$(GNUMAKE_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GNUMAKE_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(GNUMAKE_TGZ).verified: $(DOWNLOAD_DIR)/$(GNUMAKE_TGZ) \
                                         $(DOWNLOAD_DIR)/$(GNUMAKE_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(GNUMAKE_TGZ) $(DOWNLOAD_DIR)/$(GNUMAKE_SIG) $(GNUMAKE_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(GNUMAKE): $(DOWNLOAD_DIR)/$(GNUMAKE_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

clean-make:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(GNUMAKE)
