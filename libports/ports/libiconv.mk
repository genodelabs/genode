LIBICONV     = libiconv-1.14
LIBICONV_TGZ = $(LIBICONV).tar.gz
LIBICONV_URL = http://ftp.gnu.org/pub/gnu/libiconv/$(LIBICONV_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(LIBICONV)

prepare-libiconv: $(CONTRIB_DIR)/$(LIBICONV) include/iconv

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LIBICONV_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIBICONV_URL) && touch $@

$(CONTRIB_DIR)/$(LIBICONV): $(DOWNLOAD_DIR)/$(LIBICONV_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

#
# Install iconv headers
#

include/iconv:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -sf ../../src/lib/libiconv/iconv.h $@

clean-libiconv:
	$(VERBOSE)rm -rf include/iconv
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBICONV)
