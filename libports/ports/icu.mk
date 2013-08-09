ICU_VERSION = 51.2
ICU         = icu
ICU_TGZ     = $(ICU)4c-51_2-src.tgz
ICU_URL     = http://download.icu-project.org/files/icu4c/$(ICU_VERSION)/$(ICU_TGZ)
ICU_MD5     = 072e501b87065f3a0ca888f1b5165709

#
# Interface to top-level prepare Makefile
#
PORTS += $(ICU)

prepare-icu: $(CONTRIB_DIR)/$(ICU) include/icu

$(CONTRIB_DIR)/$(ICU):clean-icu

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(ICU_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(ICU_URL) && touch $@

$(DOWNLOAD_DIR)/$(ICU_TGZ).verified: $(DOWNLOAD_DIR)/$(ICU_TGZ)
	$(VERBOSE)$(HASHVERIFIER) $(DOWNLOAD_DIR)/$(ICU_TGZ) $(ICU_MD5) md5
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(ICU): $(DOWNLOAD_DIR)/$(ICU_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

include/icu:
	$(VERBOSE)mkdir -p $@
	ln -sf ../../$(CONTRIB_DIR)/$(ICU)/source/common $@/common
	ln -sf ../../$(CONTRIB_DIR)/$(ICU)/source/i18n $@/i18n

clean-icu:
	$(VERBOSE)rm -rf include/icu
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(ICU)
