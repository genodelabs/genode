FINDUTILS          = findutils-4.4.2
FINDUTILS_TGZ      = $(FINDUTILS).tar.gz
FINDUTILS_SIG      = $(FINDUTILS_TGZ).sig
FINDUTILS_BASE_URL = http://ftp.gnu.org/pub/gnu/findutils
FINDUTILS_URL      = $(FINDUTILS_BASE_URL)/$(FINDUTILS_TGZ)
FINDUTILS_URL_SIG  = $(FINDUTILS_BASE_URL)/$(FINDUTILS_SIG)
FINDUTILS_KEY      = GNU

#
# Interface to top-level prepare Makefile
#
PORTS += $(FINDUTILS)

prepare-findutils: $(CONTRIB_DIR)/$(FINDUTILS)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(FINDUTILS_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(FINDUTILS_URL) && touch $@

$(DOWNLOAD_DIR)/$(FINDUTILS_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(FINDUTILS_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(FINDUTILS_TGZ).verified: $(DOWNLOAD_DIR)/$(FINDUTILS_TGZ) \
                                           $(DOWNLOAD_DIR)/$(FINDUTILS_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(FINDUTILS_TGZ) $(DOWNLOAD_DIR)/$(FINDUTILS_SIG) $(FINDUTILS_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(FINDUTILS): $(DOWNLOAD_DIR)/$(FINDUTILS_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(FINDUTILS) -N -p1 < src/noux-pkg/findutils/build.patch

clean-findutils:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(FINDUTILS)
