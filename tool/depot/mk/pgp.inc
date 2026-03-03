#
# \brief  Helper for using the GNU privacy guard and Sequoia PGP
# \author Norman Feske
# \author Johannes Schlatow
# \date   2017-03-27
#

# Allow the gpg command to be overridden on the command line
GPG ?= gpg

# Allow the sq command to be overridden on the command line
SQ ?= sq

pubkey_filename = $(call archive_user,$1)/pubkey

pubkey_path = $(firstword \
                 $(wildcard $(addsuffix /$(call pubkey_filename,$1), \
                               $(REP_SCULPT_DEPOT_DIRS))))

# Use $(SQ) if available and compatible
_sq_cli := --cli-version 1.2.0
$(shell $(SQ) $(_sq_cli) help &> /dev/null)
ifeq ($(.SHELLSTATUS),0)

# workaround for accepting SHA1 keys of chelmuth, nfeske and cproc
_accept_sha1 := --policy-as-of 2013-01-31

# obtain key ID of 'depot/<user>/pubkey' to be used to select signing key
_pubkey_id = $(strip $(shell pubkey_file=$(call pubkey_path,$1); \
                             $(SQ) $(_sq_cli) inspect $$pubkey_file 2> /dev/null |\
                             grep Fingerprint | cut -d: -f2))

# determine whether a user's signing key is available
_sign_key_available = $(strip $(shell $(SQ) $(_sq_cli) --keyring $(call pubkey_path,$(1)) \
                                            key list --cert $(call _pubkey_id,$(1)) 2>&1 \
                                      | grep "signing"))

# determine whether a user's signing key is present as a locked softkey
_key_needs_password = $(strip $(shell $(SQ) $(_sq_cli) --keyring $(call pubkey_path,$(1)) \
                                            key list --cert $(call _pubkey_id,$(1)) \
                                      | grep @softkey | grep locked))

#
# template to be evaluated for every depot user when signing
# 
define unlock_user_key
# exit if secret key is missing
$(if $(call _sign_key_available,$(1)),,\
     $(info )
     $(info   The secret key for depot user $(1) with fingerprint)
     $(info   $(call _pubkey_id,$(1)) is missing from)
     $(info   sequoia's keystore. Please use 'sq key import' to import)
     $(info   the required secret key material.)
     $(info )
     $(error Secret key is missing.))

# prompt for password unless the key is an unlocked softkey or only available via gpg-agent
PASSWORD_$(1) := $(if $(call _key_needs_password,$(1)),\
                      $(shell read -s -p "Please enter password to decrypt the PGP key for $(1): " prompt && \
                              sq $(_sq_cli) $(_accept_sha1) --keyring $(call pubkey_path,$(1)) \
                                 sign --batch --signer $(call _pubkey_id,$(1))\
                                 --password-file <(echo -n $$prompt) --message <(echo Foobar) &>/dev/null && \
                              echo -n $$prompt),
                      $(shell true))

# print newline after password prompt
$(info )

# exit if password did not unlock the key
$(if $(subst 0,,$(.SHELLSTATUS)),$(error Wrong password))

# make password available as environment variable
export PASSWORD_$(1)
endef

# signing command to be used in sign targets
sign_target = $(SQ) $(_sq_cli) $(_accept_sha1) --keyring $(call pubkey_path,$*) \
                    sign --batch --signer $(call _pubkey_id,$*) \
                    --password-file <(echo -n $$PASSWORD_$(call archive_user,$*)) \
                    --signature-file $@ $< || \
          ( rm -f $@; false )

# verify command to be used in verify targets
verify_target = $(SQ) $(_sq_cli) $(_accept_sha1) --batch --quiet \
                      --cert-store none --key-store none \
                      verify --signer-file $$pubkey_file \
                      --signature-file $$signed_file.sig $$signed_file \
                      2> >(grep -vi authenticated)

else # use GPG
$(call check_tool,$(GPG))

# obtain key ID of 'depot/<user>/pubkey' to be used to select signing key
_pubkey_id = $(shell pubkey_file=$(call pubkey_path,$1); \
                    $(GPG) --dry-run --with-colon --import \
                           --import-options import-show $$pubkey_file 2> /dev/null |\
                    head -n 1 | cut -d: -f5)

# signing command to be used in sign targets
sign_target = $(GPG) --detach-sign --digest-algo SHA256 \
                     --batch --yes --no-tty --use-agent \
                     --local-user $(call _pubkey_id,$*) --output $@ < $< || \
              ( rm -f $@; false )

# verify command to be used in verify targets
#
# Unfortunately, gpg does not allow us to specify the armored public-key
# file directly as keyring for the verify operation. So we need to create a
# temporary dearmored version.
#
verify_target = $(GPG) --yes -o $$pubkey_file.dearmored --dearmor $$pubkey_file; \
                ( $(GPG) --no-tty --no-default-keyring \
                      --keyring $$pubkey_file.dearmored \
                      --verify $$signed_file.sig 2> /dev/null; retval=$$?; \
                  rm -f $$pubkey_file.dearmored; \
                  exit $$retval )

endif

MISSING_PUBKEY_FILES := $(sort \
                           $(foreach A,$(ARCHIVES),\
                              $(if $(call pubkey_path,$A),,\
                                 $(call archive_user,$A))))

missing_pubkey_files:
	@echo "Error: missing public-key files for:";\
	 for i in $(MISSING_PUBKEY_FILES); do echo "       $$i"; done; false

