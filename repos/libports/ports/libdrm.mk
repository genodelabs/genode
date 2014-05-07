LIBDRM_VERSION = 2.4.21
LIBDRM         = libdrm-$(LIBDRM_VERSION)
LIBDRM_DIR     = libdrm-$(LIBDRM_VERSION)
LIBDRM_TBZ2    = $(LIBDRM).tar.bz2
LIBDRM_URL     = http://dri.freedesktop.org/libdrm/$(LIBDRM_TBZ2)

#
# Interface to top-level prepare Makefile
#
PORTS += $(LIBDRM)

prepare-libdrm: $(CONTRIB_DIR)/$(LIBDRM_DIR)

$(CONTRIB_DIR)/$(LIBDRM_DIR): clean-libdrm

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LIBDRM_TBZ2):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIBDRM_URL) && touch $@

$(CONTRIB_DIR)/$(LIBDRM_DIR): $(DOWNLOAD_DIR)/$(LIBDRM_TBZ2)
	$(VERBOSE)tar xfj $< -C $(CONTRIB_DIR) && touch $@

clean-libdrm:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBDRM_DIR)

