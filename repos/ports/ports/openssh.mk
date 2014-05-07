OPENSSH          = openssh-6.1p1
OPENSSH_TGZ      = $(OPENSSH).tar.gz
OPENSSH_SIG      = $(OPENSSH_TGZ).asc
OPENSSH_BASE_URL = ftp://ftp.openbsd.org/pub/OpenBSD/OpenSSH/portable/
OPENSSH_URL      = $(OPENSSH_BASE_URL)/$(OPENSSH_TGZ)
OPENSSH_URL_SIG  = $(OPENSSH_BASE_URL)/$(OPENSSH_SIG)
OPENSSH_KEY      = 3981992A1523ABA079DBFC66CE8ECB0386FF9C48

#
# Interface to top-level prepare Makefile
#
PORTS += $(OPENSSH)

prepare-openssh: $(CONTRIB_DIR)/$(OPENSSH)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(OPENSSH_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(OPENSSH_URL) && touch $@

$(DOWNLOAD_DIR)/$(OPENSSH_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(OPENSSH_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(OPENSSH_TGZ).verified: $(DOWNLOAD_DIR)/$(OPENSSH_TGZ) \
                                         $(DOWNLOAD_DIR)/$(OPENSSH_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(OPENSSH_TGZ) $(DOWNLOAD_DIR)/$(OPENSSH_SIG) $(OPENSSH_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(OPENSSH): $(DOWNLOAD_DIR)/$(OPENSSH_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/openssh/monitor_fdpass.c.patch
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/openssh/sshconnect.h.patch
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/openssh/includes_h.patch

clean-openssh:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(OPENSSH)
