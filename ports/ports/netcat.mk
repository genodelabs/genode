GNUNETCAT     = netcat-0.7.1
GNUNETCAT_VERSION = 0.7.1
GNUNETCAT_TBZ = netcat-$(GNUNETCAT_VERSION).tar.bz2
GNUNETCAT_URL = http://downloads.sourceforge.net/sourceforge/netcat/netcat-$(GNUNETCAT_VERSION).tar.bz2

#
# Interface to top-level prepare Makefile
#
PORTS += $(GNUNETCAT)

prepare:: $(CONTRIB_DIR)/$(GNUNETCAT)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(GNUNETCAT_TBZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GNUNETCAT_URL) && touch $@

$(CONTRIB_DIR)/$(GNUNETCAT): $(DOWNLOAD_DIR)/$(GNUNETCAT_TBZ)
	$(VERBOSE)tar xfj $< -C $(CONTRIB_DIR) && touch $@

