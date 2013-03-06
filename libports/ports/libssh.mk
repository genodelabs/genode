include ports/libssh.inc

LIBSSH_TGZ = $(LIBSSH).tar.gz
LIBSSH_URL = https://red.libssh.org/attachments/download/41/$(LIBSSH_TGZ)

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

$(CONTRIB_DIR)/$(LIBSSH): $(DOWNLOAD_DIR)/$(LIBSSH_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

include/libssh:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for f in $(shell find $(CONTRIB_DIR)/$(LIBSSH)/include -name *.h); do \
		ln -sf ../../$$f $@; done

clean-libssh:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBSSH)
	$(VERBOSE)rm -rf include/libssh
