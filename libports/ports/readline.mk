READLINE          = readline-6.0
READLINE_TGZ      = $(READLINE).tar.gz
READLINE_SIG      = $(READLINE_TGZ).sig
READLINE_BASE_URL = ftp://ftp.gnu.org/gnu/readline
READLINE_URL      = $(READLINE_BASE_URL)/$(READLINE_TGZ)
READLINE_URL_SIG  = $(READLINE_BASE_URL)/$(READLINE_SIG)
READLINE_KEY      = GNU

#
# Interface to top-level prepare Makefile
#
PORTS += $(READLINE)

prepare-readline: $(CONTRIB_DIR)/$(READLINE)

$(CONTRIB_DIR)/$(READLINE): clean-readline

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(READLINE_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(READLINE_URL) && touch $@

$(DOWNLOAD_DIR)/$(READLINE_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(READLINE_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(READLINE_TGZ).verified: $(DOWNLOAD_DIR)/$(READLINE_TGZ) \
                                          $(DOWNLOAD_DIR)/$(READLINE_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(READLINE_TGZ) $(DOWNLOAD_DIR)/$(READLINE_SIG) $(READLINE_KEY)
	$(VERBOSE)touch $@

READLINE_HEADERS := rlstdc.h rltypedefs.h keymaps.h tilde.h

$(CONTRIB_DIR)/$(READLINE): $(DOWNLOAD_DIR)/$(READLINE_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@
	@# create symbolic links for public headers from contrib dir
	$(VERBOSE)for i in $(READLINE_HEADERS); do \
	  ln -sf ../../$(CONTRIB_DIR)/$(READLINE)/$$i include/readline/; done

clean-readline:
	$(VERBOSE)rm -f $(addprefix include/readline/,$(READLINE_HEADERS))
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(READLINE)
