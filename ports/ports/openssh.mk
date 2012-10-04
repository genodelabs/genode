OPENSSH     = openssh-6.1p1
OPENSSH_TGZ = $(OPENSSH).tar.gz
OPENSSH_URL = ftp://ftp.openbsd.org/pub/OpenBSD/OpenSSH/portable/$(OPENSSH).tar.gz

#
# Interface to top-level prepare Makefile
#
PORTS += $(OPENSSH)

prepare:: $(CONTRIB_DIR)/$(OPENSSH)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(OPENSSH_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(OPENSSH_URL) && touch $@

$(CONTRIB_DIR)/$(OPENSSH): $(DOWNLOAD_DIR)/$(OPENSSH_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/openssh/monitor_fdpass.c.patch
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/openssh/sshconnect.h.patch
	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/openssh/includes_h.patch
