include ports/libssh.inc

LIBSSH_TGZ     = $(LIBSSH).tar.gz
LIBSSH_SIG     = $(LIBSSH).tar.asc
LIBSSH_URL     = https://red.libssh.org/attachments/download/41/$(LIBSSH_TGZ)
LIBSSH_URL_SIG = https://red.libssh.org/attachments/download/42/$(LIBSSH_SIG)

#
# XXX The signature check for libssh is prepared but yet disabled. The source
#     for the verification PGP key is yet unknown. If the PGP key becomes
#     known, just add it to the _KEY variable.
#
LIBSSH_KEY = FIXME

#
# Interface to top-level prepare Makefile
#
PORTS += $(LIBSSH)

prepare-libssh: $(CONTRIB_DIR)/$(LIBSSH) include/libssh

$(CONTRIB_DIR)/$(LIBSSH): clean-libssh

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LIBSSH_TGZ):
	$(VERBOSE)wget --no-check-certificate -c -P $(DOWNLOAD_DIR) $(LIBSSH_URL) && touch $@

$(DOWNLOAD_DIR)/$(LIBSSH_SIG):
	$(VERBOSE)wget --no-check-certificate -c -P $(DOWNLOAD_DIR) $(LIBSSH_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(LIBSSH_TGZ).verified: $(DOWNLOAD_DIR)/$(LIBSSH_TGZ) \
                                        $(DOWNLOAD_DIR)/$(LIBSSH_SIG)
	# XXX We have no key for libssh at the moment
	#$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(LIBSSH_TGZ) $(DOWNLOAD_DIR)/$(LIBSSH_SIG) $(LIBSSH_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(LIBSSH): $(DOWNLOAD_DIR)/$(LIBSSH_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

include/libssh:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for f in $(shell find $(CONTRIB_DIR)/$(LIBSSH)/include -name *.h); do \
		ln -sf ../../$$f $@; done

clean-libssh:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBSSH)
	$(VERBOSE)rm -rf include/libssh
