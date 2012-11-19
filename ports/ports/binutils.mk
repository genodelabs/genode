BINUTILS      = binutils-2.22
BINUTILS_TBZ2 = $(BINUTILS).tar.bz2
BINUTILS_URL  = ftp://ftp.fu-berlin.de/gnu/binutils/$(BINUTILS_TBZ2)

#
# Interface to top-level prepare Makefile
#
PORTS += $(BINUTILS)

prepare:: $(CONTRIB_DIR)/$(BINUTILS)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(BINUTILS_TBZ2):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(BINUTILS_URL) && touch $@

$(CONTRIB_DIR)/$(BINUTILS): $(DOWNLOAD_DIR)/$(BINUTILS_TBZ2)
	$(VERBOSE)tar xfj $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(BINUTILS) -N -p1 < src/noux-pkg/binutils/build.patch
