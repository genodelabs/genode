BASH     = bash-4.1
BASH_TGZ = $(BASH).tar.gz
BASH_URL = http://ftp.gnu.org/gnu/bash/$(BASH_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(BASH)

prepare:: $(CONTRIB_DIR)/$(BASH)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(BASH_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(BASH_URL) && touch $@

$(CONTRIB_DIR)/$(BASH): $(DOWNLOAD_DIR)/$(BASH_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -N -p0 < src/noux-pkg/bash/build.patch

