DASH     = dash-0.5.6
DASH_TGZ = $(DASH).tar.gz
DASH_URL = http://gondor.apana.org.au/~herbert/dash/files/$(DASH_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(DASH)

prepare-dash: $(CONTRIB_DIR)/$(DASH)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(DASH_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(DASH_URL) && touch $@

$(CONTRIB_DIR)/$(DASH): $(DOWNLOAD_DIR)/$(DASH_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -N -p0 < src/noux-pkg/dash/build.patch

clean-dash:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(DASH)

