include ports/lighttpd.inc

LIGHTTPD_TGZ = $(LIGHTTPD).tar.gz
LIGHTTPD_URL = http://download.lighttpd.net/lighttpd/releases-1.4.x/$(LIGHTTPD_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(LIGHTTPD)

prepare:: $(CONTRIB_DIR)/$(LIGHTTPD)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LIGHTTPD_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIGHTTPD_URL) && touch $@

$(CONTRIB_DIR)/$(LIGHTTPD): $(DOWNLOAD_DIR)/$(LIGHTTPD_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -N -p1 < src/app/lighttpd/disable_gethostbyaddr_fcntl.patch
