BINUTILS          = binutils-2.22
BINUTILS_TBZ2     = $(BINUTILS).tar.bz2
BINUTILS_SIG      = $(BINUTILS_TBZ2).sig
BINUTILS_BASE_URL = ftp://ftp.fu-berlin.de/gnu/binutils
BINUTILS_URL      = $(BINUTILS_BASE_URL)/$(BINUTILS_TBZ2)
BINUTILS_URL_SIG  = $(BINUTILS_BASE_URL)/$(BINUTILS_SIG)
BINUTILS_KEY      = GNU

#
# Interface to top-level prepare Makefile
#
PORTS += $(BINUTILS)

prepare-binutils: $(CONTRIB_DIR)/$(BINUTILS)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(BINUTILS_TBZ2):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(BINUTILS_URL) && touch $@

$(DOWNLOAD_DIR)/$(BINUTILS_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(BINUTILS_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(BINUTILS_TBZ2).verified: $(DOWNLOAD_DIR)/$(BINUTILS_TBZ2) \
                                           $(DOWNLOAD_DIR)/$(BINUTILS_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(BINUTILS_TBZ2) $(DOWNLOAD_DIR)/$(BINUTILS_SIG) $(BINUTILS_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(BINUTILS): $(DOWNLOAD_DIR)/$(BINUTILS_TBZ2).verified
	$(VERBOSE)tar xfj $(<:.verified=) -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(BINUTILS) -N -p1 < src/noux-pkg/binutils/build.patch

clean-binutils:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(BINUTILS)
