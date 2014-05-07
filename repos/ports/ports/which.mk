WHICH          = which-2.20
WHICH_TGZ      = $(WHICH).tar.gz
WHICH_SIG      = $(WHICH_TGZ).sig
WHICH_BASE_URL = http://ftp.gnu.org/gnu/which
WHICH_URL      = $(WHICH_BASE_URL)/$(WHICH_TGZ)
WHICH_URL_SIG  = $(WHICH_BASE_URL)/$(WHICH_SIG)
WHICH_KEY      = GNU

#
# Interface to top-level prepare Makefile
#
PORTS += $(WHICH)

prepare-which: $(CONTRIB_DIR)/$(WHICH)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(WHICH_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) -O $@ $(WHICH_URL) && touch $@

$(DOWNLOAD_DIR)/$(WHICH_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(WHICH_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(WHICH_TGZ).verified: $(DOWNLOAD_DIR)/$(WHICH_TGZ) \
                                       $(DOWNLOAD_DIR)/$(WHICH_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(WHICH_TGZ) $(DOWNLOAD_DIR)/$(WHICH_SIG) $(WHICH_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(WHICH): $(DOWNLOAD_DIR)/$(WHICH_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

clean-which:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(WHICH)
