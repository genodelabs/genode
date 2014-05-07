BASH          = bash-4.1
BASH_TGZ      = $(BASH).tar.gz
BASH_SIG      = $(BASH_TGZ).sig
BASH_BASE_URL = http://ftp.gnu.org/gnu/bash
BASH_URL      = $(BASH_BASE_URL)/$(BASH_TGZ)
BASH_URL_SIG  = $(BASH_BASE_URL)/$(BASH_SIG)
BASH_KEY      = GNU

#
# Interface to top-level prepare Makefile
#
PORTS += $(BASH)

prepare-bash: $(CONTRIB_DIR)/$(BASH)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(BASH_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(BASH_URL) && touch $@

$(DOWNLOAD_DIR)/$(BASH_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(BASH_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(BASH_TGZ).verified: $(DOWNLOAD_DIR)/$(BASH_TGZ) \
                                      $(DOWNLOAD_DIR)/$(BASH_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(BASH_TGZ) $(DOWNLOAD_DIR)/$(BASH_SIG) $(BASH_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(BASH): $(DOWNLOAD_DIR)/$(BASH_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(BASH) -N -p1 < src/noux-pkg/bash/build.patch
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(BASH) -N -p1 < src/noux-pkg/bash/check_dev_tty.patch

clean-bash:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(BASH)
