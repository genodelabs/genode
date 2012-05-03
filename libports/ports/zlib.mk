ZLIB     = zlib-1.2.7
ZLIB_TGZ = $(ZLIB).tar.gz
ZLIB_URL = http://zlib.net/$(ZLIB_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(ZLIB)

prepare-zlib: $(CONTRIB_DIR)/$(ZLIB) include/zlib

$(CONTRIB_DIR)/$(ZLIB):clean-zlib

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(ZLIB_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(ZLIB_URL) && touch $@

$(CONTRIB_DIR)/$(ZLIB): $(DOWNLOAD_DIR)/$(ZLIB_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

ZLIB_INCLUDES = zconf.h zlib.h

include/zlib:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for i in $(ZLIB_INCLUDES); do \
		ln -sf ../../$(CONTRIB_DIR)/$(ZLIB)/$$i $@; done

clean-zlib:
	$(VERBOSE)rm -rf include/zlib
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(ZLIB)
