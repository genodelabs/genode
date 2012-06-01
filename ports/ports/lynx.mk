LYNX = lynx-2.8.8.dev12
LYNX_TGZ = $(LYNX).tar.gz
LYNX_URL = http://lynx.isc.org/gnumatic/$(LYNX_TGZ)
#
# Interface to top-level prepare Makefile
#
PORTS += $(LYNX)

prepare:: $(CONTRIB_DIR)/$(LYNX)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LYNX_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) -O $@ $(LYNX_URL) && touch $@

$(CONTRIB_DIR)/$(LYNX): $(DOWNLOAD_DIR)/$(LYNX_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/lynx/build.patch
