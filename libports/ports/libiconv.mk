LIBICONV          = libiconv-1.14
LIBICONV_TGZ      = $(LIBICONV).tar.gz
LIBICONV_SIG      = $(LIBICONV_TGZ).sig
LIBICONV_BASE_URL = http://ftp.gnu.org/pub/gnu/libiconv
LIBICONV_URL      = $(LIBICONV_BASE_URL)//$(LIBICONV_TGZ)
LIBICONV_URL_SIG  = $(LIBICONV_BASE_URL)//$(LIBICONV_SIG)
LIBICONV_KEY      = GNU

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

$(DOWNLOAD_DIR)/$(LIBICONV_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIBICONV_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(LIBICONV_TGZ).verified: $(DOWNLOAD_DIR)/$(LIBICONV_TGZ) \
                                          $(DOWNLOAD_DIR)/$(LIBICONV_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(LIBICONV_TGZ) $(DOWNLOAD_DIR)/$(LIBICONV_SIG) $(LIBICONV_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(LIBICONV): $(DOWNLOAD_DIR)/$(LIBICONV_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

#
# Install iconv headers
#

include/iconv:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -sf ../../src/lib/libiconv/iconv.h $@

clean-libiconv:
	$(VERBOSE)rm -rf include/iconv
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBICONV)
