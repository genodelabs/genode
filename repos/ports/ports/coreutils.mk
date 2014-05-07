COREUTILS          = coreutils-8.9
COREUTILS_TGZ      = $(COREUTILS).tar.gz
COREUTILS_SIG      = $(COREUTILS_TGZ).sig
COREUTILS_BASE_URL = http://ftp.gnu.org/gnu/coreutils
COREUTILS_URL      = $(COREUTILS_BASE_URL)/$(COREUTILS_TGZ)
COREUTILS_URL_SIG  = $(COREUTILS_BASE_URL)/$(COREUTILS_SIG)
COREUTILS_KEY      = GNU

#
# Interface to top-level prepare Makefile
#
PORTS += $(COREUTILS)

prepare-coreutils: $(CONTRIB_DIR)/$(COREUTILS)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(COREUTILS_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(COREUTILS_URL) && touch $@

$(DOWNLOAD_DIR)/$(COREUTILS_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(COREUTILS_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(COREUTILS_TGZ).verified: $(DOWNLOAD_DIR)/$(COREUTILS_TGZ) \
                                           $(DOWNLOAD_DIR)/$(COREUTILS_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(COREUTILS_TGZ) $(DOWNLOAD_DIR)/$(COREUTILS_SIG) $(COREUTILS_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(COREUTILS): $(DOWNLOAD_DIR)/$(COREUTILS_TGZ)
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

clean-coreutils:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(COREUTILS)
