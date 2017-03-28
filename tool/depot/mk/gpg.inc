#
# \brief  Helper for using the GNU privacy guard
# \author Norman Feske
# \date   2017-03-27
#

pubkey_filename = $(call archive_user,$1)/pubkey
pubkey_path     = $(wildcard $(DEPOT_DIR)/$(call pubkey_filename,$1))

# obtain key ID of 'depot/<user>/pubkey' to be used to select signing key
pubkey_id = $(shell gpg --with-colon < $(call pubkey_path,$1) | head -1 | cut -d: -f5 )

MISSING_PUBKEY_FILES := $(sort \
                           $(foreach A,$(ARCHIVES),\
                              $(if $(call pubkey_path,$A),,\
                                 $(DEPOT_DIR)/$(call pubkey_filename,$A))))

missing_pubkey_files:
	@echo "Error: missing public-key files:";\
	 for i in $(MISSING_PUBKEY_FILES); do echo "       $$i"; done; false

