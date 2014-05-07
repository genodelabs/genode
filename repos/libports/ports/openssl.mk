OPENSSL_VERSION  = 1.0.1g
OPENSSL          = openssl-$(OPENSSL_VERSION)
OPENSSL_TGZ      = $(OPENSSL).tar.gz
OPENSSL_SIG      = $(OPENSSL_TGZ).asc
OPENSSL_BASE_URL = https://www.openssl.org/source
OPENSSL_URL      = $(OPENSSL_BASE_URL)/$(OPENSSL_TGZ)
OPENSSL_URL_SIG  = $(OPENSSL_BASE_URL)/$(OPENSSL_SIG)
OPENSSL_KEY      = "49A563D9 26BB437D F295C759 9C58A66D 2118CF83 F709453B 5A6A9B85"

# local openssl src
OPENSSL_SRC      = src/lib/openssl

#
# Interface to top-level prepare Makefile
#
PORTS += $(OPENSSL)

prepare-openssl: $(CONTRIB_DIR)/$(OPENSSL) include/openssl generate_asm

#$(CONTRIB_DIR)/$(OPENSSL):

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(OPENSSL_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(OPENSSL_URL) && touch $@

$(DOWNLOAD_DIR)/$(OPENSSL_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(OPENSSL_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(OPENSSL_TGZ).verified: $(DOWNLOAD_DIR)/$(OPENSSL_TGZ) \
                                         $(DOWNLOAD_DIR)/$(OPENSSL_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(OPENSSL_TGZ) $(DOWNLOAD_DIR)/$(OPENSSL_SIG) $(OPENSSL_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(OPENSSL): $(DOWNLOAD_DIR)/$(OPENSSL_TGZ)
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

#
# Generate ASM codes
#

generate_asm: $(OPENSSL_SRC)/x86_64/modexp512.s $(OPENSSL_SRC)/x86_64/rc4_md5.s

$(OPENSSL_SRC)/x86_64/modexp512.s:
	$(VERBOSE)perl $(CONTRIB_DIR)/$(OPENSSL)/crypto/bn/asm/modexp512-x86_64.pl \
		$(CONTRIB_DIR)/$(OPENSSL_DIR)/crypto/perlasm/x86as.pl > $@

$(OPENSSL_SRC)/x86_64/rc4_md5.s:
	$(VERBOSE)perl $(CONTRIB_DIR)/$(OPENSSL)/crypto/rc4/asm/rc4-md5-x86_64.pl \
		$(CONTRIB_DIR)/$(OPENSSL_DIR)/crypto/perlasm/x86as.pl > $@


#
# Install openssl headers
#
include/openssl:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for i in `find $(CONTRIB_DIR)/$(OPENSSL)/include -name *.h`; do \
		ln -fs ../../$$i include/openssl/; done
	$(VERBOSE)rm include/openssl/opensslconf.h
	$(VERBOSE)ln -fs ../../$(CONTRIB_DIR)/$(OPENSSL)/e_os.h include/openssl/
	$(VERBOSE)ln -fs ../../$(CONTRIB_DIR)/$(OPENSSL)/crypto/md2/md2.h include/openssl/
	$(VERBOSE)ln -fs ../../$(CONTRIB_DIR)/$(OPENSSL)/crypto/rc5/rc5.h include/openssl/
	$(VERBOSE)ln -fs ../../$(CONTRIB_DIR)/$(OPENSSL)/crypto/store/store.h include/openssl/

clean-openssl:
	$(VERBOSE)rm -rf include/openssl
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(OPENSSL)
	$(VERBOSE)rm -rf $(OPENSSL_SRC)/x86_32/*.s $(OPENSSL_SRC)/x86_64/*.s
