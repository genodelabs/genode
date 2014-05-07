ZLIB_VERSION = 1.2.8
ZLIB         = zlib-$(ZLIB_VERSION)
ZLIB_TGZ     = $(ZLIB).tar.gz
ZLIB_URL     = http://downloads.sourceforge.net/project/libpng/zlib/$(ZLIB_VERSION)/$(ZLIB_TGZ)
ZLIB_MD5     = 44d667c142d7cda120332623eab69f40

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

$(DOWNLOAD_DIR)/$(ZLIB_TGZ).verified: $(DOWNLOAD_DIR)/$(ZLIB_TGZ)
	$(VERBOSE)$(HASHVERIFIER) $(DOWNLOAD_DIR)/$(ZLIB_TGZ) $(ZLIB_MD5) md5
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(ZLIB): $(DOWNLOAD_DIR)/$(ZLIB_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

ZLIB_INCLUDES = zconf.h zlib.h

include/zlib:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for i in $(ZLIB_INCLUDES); do \
		ln -sf ../../$(CONTRIB_DIR)/$(ZLIB)/$$i $@; done

clean-zlib:
	$(VERBOSE)rm -rf include/zlib
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(ZLIB)
