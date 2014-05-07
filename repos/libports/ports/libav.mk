include lib/import/import-av.inc

LIBAV_TGZ      = $(LIBAV).tar.gz
LIBAV_SIG      = $(LIBAV_TGZ).asc
LIBAV_SHA      = $(LIBAV_TGZ).sha1
LIBAV_BASE_URL = http://www.libav.org/releases
LIBAV_URL      = $(LIBAV_BASE_URL)/$(LIBAV_TGZ)
LIBAV_URL_SIG  = $(LIBAV_BASE_URL)/$(LIBAV_SIG)
LIBAV_URL_SHA  = $(LIBAV_BASE_URL)/$(LIBAV_SHA)

#
# XXX Add hash verification for libav downloads. Note: signatures are provided
#     for newer versions. Therefore, all is prepared in the make file for
#     enabling the signature check. However, the signature check is yet
#     commented out.
#
#LIBAV_KEY     = UNCLEAR WHAT GOES IN HERE

#
# Interface to top-level prepare Makefile
#
PORTS += $(LIBAV)

prepare-libav: $(CONTRIB_DIR)/$(LIBAV)

$(CONTRIB_DIR)/$(LIBAV): clean-libav

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LIBAV_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIBAV_URL) && touch $@

$(DOWNLOAD_DIR)/$(LIBAV_SHA):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIBAV_URL_SHA) && touch $@

$(DOWNLOAD_DIR)/$(LIBAV_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIBAV_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(LIBAV_TGZ).verified: $(DOWNLOAD_DIR)/$(LIBAV_TGZ) \
                                       $(DOWNLOAD_DIR)/$(LIBAV_SHA) \
                                       $(DOWNLOAD_DIR)/$(LIBAV_SIG)
	# XXX Hash verification of libav does not ensure authenticity
	$(VERBOSE)$(HASHVERIFIER) $(DOWNLOAD_DIR)/$(LIBAV_TGZ) $(DOWNLOAD_DIR)/$(LIBAV_SHA) sha1
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(LIBAV): $(DOWNLOAD_DIR)/$(LIBAV_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(LIBAV) -p1 -i $(CURDIR)/src/app/avplay/avplay.patch

clean-libav:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBAV)
