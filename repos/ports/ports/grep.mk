GNUGREP     = grep-2.14
GNUGREP_TXZ = $(GNUGREP).tar.xz
GNUGREP_SIG = $(GNUGREP_TXZ).sig
GNUGREP_URL = http://ftp.gnu.org/pub/gnu/grep
GNUGREP_KEY = GNU

#
# Interface to top-level prepare Makefile
#
PORTS += $(GNUGREP)

prepare-grep: $(CONTRIB_DIR)/$(GNUGREP)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(GNUGREP_TXZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GNUGREP_URL)/$(GNUGREP_TXZ) && touch $@

$(DOWNLOAD_DIR)/$(GNUGREP_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GNUGREP_URL)/$(GNUGREP_SIG) && touch $@

$(CONTRIB_DIR)/$(GNUGREP): $(DOWNLOAD_DIR)/$(GNUGREP_TXZ).verified
	$(VERBOSE)tar xfJ $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

$(DOWNLOAD_DIR)/$(GNUGREP_TXZ).verified: $(DOWNLOAD_DIR)/$(GNUGREP_TXZ) \
                                         $(DOWNLOAD_DIR)/$(GNUGREP_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(GNUGREP_TXZ) $(DOWNLOAD_DIR)/$(GNUGREP_SIG) $(GNUGREP_KEY)
	$(VERBOSE)touch $@

clean-grep:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(GNUGREP)
