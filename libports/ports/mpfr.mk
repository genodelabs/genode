MPFR          = mpfr-3.0.0
MPFR_TGZ      = $(MPFR).tar.gz
MPFR_SIG      = $(MPFR_TGZ).asc
MPFR_BASE_URL = http://www.mpfr.org/$(MPFR)
MPFR_URL      = $(MPFR_BASE_URL)/$(MPFR_TGZ)
MPFR_URL_SIG  = $(MPFR_BASE_URL)/$(MPFR_SIG)
MPFR_KEY      = GNU

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

$(DOWNLOAD_DIR)/$(MPFR_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(MPFR_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(MPFR_TGZ).verified: $(DOWNLOAD_DIR)/$(MPFR_TGZ) \
                                      $(DOWNLOAD_DIR)/$(MPFR_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(MPFR_TGZ) $(DOWNLOAD_DIR)/$(MPFR_SIG) $(MPFR_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(MPFR): $(DOWNLOAD_DIR)/$(MPFR_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

include/mpfr/mpfr.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../$(CONTRIB_DIR)/$(MPFR)/mpfr.h $@

include/mpfr/mparam.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../$(CONTRIB_DIR)/$(MPFR)/mparam_h.in $@

clean-mpfr:
	$(VERBOSE)rm -f  $(MPFR_INCLUDES)
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(MPFR)
