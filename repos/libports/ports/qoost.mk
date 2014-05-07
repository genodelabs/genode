QOOST_VERSION = 2013-06-10
QOOST         = qoost-$(QOOST_VERSION)
QOOST_TBZ     = $(QOOST).tar.bz2
QOOST_URL     = https://downloads.sourceforge.net/project/qoost/$(QOOST_TBZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(QOOST)

prepare-qoost: $(CONTRIB_DIR)/$(QOOST) include/qoost

$(CONTRIB_DIR)/$(QOOST): clean-qoost

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(QOOST_TBZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(QOOST_URL) && touch $@

$(CONTRIB_DIR)/$(QOOST): $(DOWNLOAD_DIR)/$(QOOST_TBZ)
	$(VERBOSE)tar xfj $< -C $(CONTRIB_DIR) && touch $@

include/qoost:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../$(CONTRIB_DIR)/$(QOOST)/include/qoost $@


clean-qoost:
	$(VERBOSE)rm -f  include/qoost
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(QOOST)
