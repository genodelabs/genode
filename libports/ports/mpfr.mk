MPFR     = mpfr-3.0.0
MPFR_TGZ = $(MPFR).tar.gz
MPFR_URL = http://www.mpfr.org/$(MPFR)/$(MPFR_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(MPFR)

MPFR_INCLUDES = include/mpfr/mpfr.h include/mpfr/mparam.h

prepare-mpfr: $(CONTRIB_DIR)/$(MPFR) $(MPFR_INCLUDES)

$(CONTRIB_DIR)/$(MPFR): clean-mpfr

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(MPFR_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(MPFR_URL) && touch $@

$(CONTRIB_DIR)/$(MPFR): $(DOWNLOAD_DIR)/$(MPFR_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

include/mpfr/mpfr.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../$(CONTRIB_DIR)/$(MPFR)/mpfr.h $@

include/mpfr/mparam.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../$(CONTRIB_DIR)/$(MPFR)/mparam_h.in $@

clean-mpfr:
	$(VERBOSE)rm -f  $(MPFR_INCLUDES)
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(MPFR)
