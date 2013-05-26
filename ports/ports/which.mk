WHICH          = which-2.20
WHICH_TGZ      = $(WHICH).tar.gz
WHICH_SIG      = $(WHICH_TGZ).sig
WHICH_BASE_URL = http://ftp.gnu.org/gnu/which
WHICH_URL      = $(WHICH_BASE_URL)/$(WHICH_TGZ)
WHICH_URL_SIG  = $(WHICH_BASE_URL)/$(WHICH_SIG)
WHICH_KEY      = GNU
#
# Interface to top-level prepare Makefile
#
PORTS += $(WHICH)

prepare:: $(CONTRIB_DIR)/$(WHICH)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(WHICH_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) -O $@ $(WHICH_URL) && touch $@
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(WHICH_URL_SIG) && touch $@
	$(VERBOSE)$(SIGVERIFIER) $@ $(DOWNLOAD_DIR)/$(WHICH_SIG) $(WHICH_KEY)

$(CONTRIB_DIR)/$(WHICH): $(DOWNLOAD_DIR)/$(WHICH_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
#	$(VERBOSE)patch -d contrib/ -N -p0 < src/noux-pkg/lynx/build.patch
