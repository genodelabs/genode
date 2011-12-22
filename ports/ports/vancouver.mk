VANCOUVER_VERSION = 0.4
VANCOUVER         = vancouver-$(VANCOUVER_VERSION)
VANCOUVER_TBZ2    = $(VANCOUVER).tar.bz2
VANCOUVER_URL     = http://os.inf.tu-dresden.de/~jsteckli/nova/nova-userland-$(VANCOUVER_VERSION).tar.bz2

#
# Interface to top-level prepare Makefile
#
PORTS += $(VANCOUVER)

prepare:: $(CONTRIB_DIR)/$(VANCOUVER)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(VANCOUVER_TBZ2):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) -O $@ $(VANCOUVER_URL) && touch $@

$(CONTRIB_DIR)/$(VANCOUVER): $(DOWNLOAD_DIR)/$(VANCOUVER_TBZ2)
	$(VERBOSE)tar xfj $< --transform "s/nova-userland/vancouver/" -C $(CONTRIB_DIR)

