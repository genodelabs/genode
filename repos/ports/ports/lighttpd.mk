include ports/lighttpd.inc

LIGHTTPD_TGZ      = $(LIGHTTPD).tar.gz
LIGHTTPD_SIG      = $(LIGHTTPD_TGZ).asc
LIGHTTPD_BASE_URL = http://download.lighttpd.net/lighttpd/releases-1.4.x
LIGHTTPD_URL      = $(LIGHTTPD_BASE_URL)/$(LIGHTTPD_TGZ)
LIGHTTPD_URL_SIG  = $(LIGHTTPD_BASE_URL)/$(LIGHTTPD_SIG)
LIGHTTPD_KEY      = stbuehler@lighttpd.net

#
# Interface to top-level prepare Makefile
#
PORTS += $(LIGHTTPD)

prepare-lighttpd: $(CONTRIB_DIR)/$(LIGHTTPD)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LIGHTTPD_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIGHTTPD_URL) && touch $@

$(DOWNLOAD_DIR)/$(LIGHTTPD_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIGHTTPD_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(LIGHTTPD_TGZ).verified: $(DOWNLOAD_DIR)/$(LIGHTTPD_TGZ) \
                                          $(DOWNLOAD_DIR)/$(LIGHTTPD_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(LIGHTTPD_TGZ) $(DOWNLOAD_DIR)/$(LIGHTTPD_SIG) $(LIGHTTPD_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(LIGHTTPD): $(DOWNLOAD_DIR)/$(LIGHTTPD_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -N -p1 < src/app/lighttpd/disable_gethostbyaddr_fcntl.patch

clean-lighttpd:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIGHTTPD)
