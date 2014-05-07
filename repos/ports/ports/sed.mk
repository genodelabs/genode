GNUSED     = sed-4.2.2
GNUSED_TGZ = $(GNUSED).tar.gz
GNUSED_SIG = $(GNUSED_TGZ).sig
GNUSED_URL = http://ftp.gnu.org/pub/gnu/sed
GNUSED_KEY = GNU

#
# Interface to top-level prepare Makefile
#
PORTS += $(GNUSED)

prepare-sed: $(CONTRIB_DIR)/$(GNUSED)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(GNUSED_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GNUSED_URL)/$(GNUSED_TGZ) && touch $@

$(DOWNLOAD_DIR)/$(GNUSED_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GNUSED_URL)/$(GNUSED_SIG) && touch $@

$(CONTRIB_DIR)/$(GNUSED): $(DOWNLOAD_DIR)/$(GNUSED_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

$(DOWNLOAD_DIR)/$(GNUSED_TGZ).verified: $(DOWNLOAD_DIR)/$(GNUSED_TGZ) \
                                        $(DOWNLOAD_DIR)/$(GNUSED_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(GNUSED_TGZ) $(DOWNLOAD_DIR)/$(GNUSED_SIG) $(GNUSED_KEY)
	$(VERBOSE)touch $@

clean-sed:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(SED)

