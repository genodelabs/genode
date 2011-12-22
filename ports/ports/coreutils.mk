COREUTILS     = coreutils-8.9
COREUTILS_TGZ = $(COREUTILS).tar.gz
COREUTILS_URL = http://ftp.gnu.org/gnu/coreutils/$(COREUTILS_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(COREUTILS)

prepare:: $(CONTRIB_DIR)/$(COREUTILS)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(COREUTILS_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(COREUTILS_URL) && touch $@

$(CONTRIB_DIR)/$(COREUTILS): $(DOWNLOAD_DIR)/$(COREUTILS_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

