LYNX         = lynx-2.8.8.dev12
LYNX_TGZ     = $(LYNX).tar.gz
LYNX_SIG     = $(LYNX_TGZ).asc
LYNX_URL     = http://lynx.isc.org/gnumatic/$(LYNX_TGZ)
LYNX_URL_SIG = UNKOWN/$(LYNX_SIG)
LYNX_KEY     = dickey@sf1.isc.org

#
# Interface to top-level prepare Makefile
#
PORTS += $(LYNX)

prepare-lynx: $(CONTRIB_DIR)/$(LYNX)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LYNX_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) -O $@ $(LYNX_URL) && touch $@

$(DOWNLOAD_DIR)/$(LYNX_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LYNX_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(LYNX_TGZ).verified: $(DOWNLOAD_DIR)/$(LYNX_TGZ)
	#
	# XXX The current source of the lynx tarball does not contain the signature
	#     file. The official location contains the signature. Thus, upon
	#     switching to the official location, the signature check can be
	#     enabled.
	#
	#$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(LYNX_TGZ) $(DOWNLOAD_DIR)/$(LYNX_SIG) $(LYNX_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(LYNX): $(DOWNLOAD_DIR)/$(LYNX_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/lynx/build.patch

clean-lynx:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LYNX)
