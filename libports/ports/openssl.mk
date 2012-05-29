OPENSSL_VERSION = 1.0.1b
OPENSSL         = openssl-$(OPENSSL_VERSION)
OPENSSL_TGZ     = $(OPENSSL).tar.gz
OPENSSL_URL     = https://www.openssl.org/source/$(OPENSSL_TGZ)

# local openssl src
OPENSSL_SRC     = src/lib/openssl

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

$(CONTRIB_DIR)/$(OPENSSL): $(DOWNLOAD_DIR)/$(OPENSSL_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

#
# Generate ASM codes
#

generate_asm: $(OPENSSL_SRC)/x86_32/cpuid.s $(OPENSSL_SRC)/x86_64/cpuid.s \
	      $(OPENSSL_SRC)/x86_32/aes_enc.s $(OPENSSL_SRC)/x86_64/aes_enc.s \
	      $(OPENSSL_SRC)/x86_64/modexp512.s $(OPENSSL_SRC)/x86_64/rc4_md5.s

$(OPENSSL_SRC)/x86_32/cpuid.s:
	$(VERBOSE)perl $(CONTRIB_DIR)/$(OPENSSL)/crypto/x86cpuid.pl elf \
		$(CONTRIB_DIR)/$(OPENSSEL)/crypto/perlasm/x86asm.pl elf > $@

$(OPENSSL_SRC)/x86_64/cpuid.s:
	$(VERBOSE)perl $(CONTRIB_DIR)/$(OPENSSL)/crypto/x86_64cpuid.pl elf \
		$(CONTRIB_DIR)/$(OPENSSEL)/crypto/perlasm/x86asm.pl elf > $@

$(OPENSSL_SRC)/x86_32/aes_enc.s:
	$(VERBOSE)perl $(CONTRIB_DIR)/$(OPENSSL)/crypto/aes/asm/aes-586.pl elf \
		$(CONTRIB_DIR)/$(OPENSSEL)/crypto/perlasm/x86asm.pl elf > $@

$(OPENSSL_SRC)/x86_64/aes_enc.s:
	$(VERBOSE)perl $(CONTRIB_DIR)/$(OPENSSL)/crypto/aes/asm/aes-x86_64.pl elf \
		$(CONTRIB_DIR)/$(OPENSSEL)/crypto/perlasm/x86asm.pl elf > $@

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
	$(VERBOSE)rm -rf include/OPENSSL
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(OPENSSL)
	$(VERBOSE)rm -rf $(OPENSSL_SRC)/x86_32/*.s $(OPENSSL_SRC)/x86_64/*.s
