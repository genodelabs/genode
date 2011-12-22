LIBPNG     = libpng-1.4.1
LIBPNG_TGZ = libpng-1.4.1.tar.gz
LIBPNG_URL = http://prdownloads.sourceforge.net/libpng/$(LIBPNG_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(LIBPNG)

prepare-libpng: $(CONTRIB_DIR)/$(LIBPNG) include/libpng

$(CONTRIB_DIR)/$(LIBPNG): clean-libpng

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LIBPNG_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIBPNG_URL) && touch $@

$(CONTRIB_DIR)/$(LIBPNG): $(DOWNLOAD_DIR)/$(LIBPNG_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

LIBPNG_INCLUDES = pngconf.h png.h pngpriv.h

include/libpng:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for i in $(LIBPNG_INCLUDES); do \
		ln -sf ../../$(CONTRIB_DIR)/$(LIBPNG)/$$i $@; done

clean-libpng:
	$(VERBOSE)rm -rf include/libpng
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBPNG)
