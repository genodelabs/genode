#
# \brief  Helper for using the GNU privacy guard
# \author Norman Feske
# \date   2017-03-27
#

# Allow the gpg command to be overridden on the command line
GPG ?= gpg

pubkey_filename = $(call archive_user,$1)/pubkey
pubkey_path     = $(wildcard $(DEPOT_DIR)/$(call pubkey_filename,$1))

# obtain key ID of 'depot/<user>/pubkey' to be used to select signing key
pubkey_id = $(shell pubkey_file=$(call pubkey_path,$1); \
                    $(GPG) --yes -o $$pubkey_file.dearmored --dearmor $$pubkey_file; \
                    $(GPG) --with-colon --no-default-keyring --list-public-keys \
                           --keyring $$pubkey_file.dearmored |\
                    head -n 2 | tail -n 1 | cut -d: -f5; rm -f $$pubkey_file.dearmored)

MISSING_PUBKEY_FILES := $(sort \
                           $(foreach A,$(ARCHIVES),\
                              $(if $(call pubkey_path,$A),,\
                                 $(DEPOT_DIR)/$(call pubkey_filename,$A))))

missing_pubkey_files:
	@echo "Error: missing public-key files:";\
	 for i in $(MISSING_PUBKEY_FILES); do echo "       $$i"; done; false

