FINDUTILS     = findutils-4.4.2
FINDUTILS_TGZ = $(FINDUTILS).tar.gz
FINDUTILS_URL = http://ftp.gnu.org/pub/gnu/findutils/$(FINDUTILS_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(FINDUTILS)

prepare:: $(CONTRIB_DIR)/$(FINDUTILS)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(FINDUTILS_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(FINDUTILS_URL) && touch $@

$(CONTRIB_DIR)/$(FINDUTILS): $(DOWNLOAD_DIR)/$(FINDUTILS_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(FINDUTILS) -N -p1 < src/noux-pkg/findutils/build.patch
