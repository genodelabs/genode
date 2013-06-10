include lib/import/import-mpc.mk

MPC_TGZ      = $(MPC).tar.gz
MPC_SIG      = $(MPC_TGZ).asc
MPC_BASE_URL = http://www.multiprecision.org/mpc/download
MPC_URL      = $(MPC_BASE_URL)/$(MPC_TGZ)
MPC_URL_SIG  = $(MPC_BASE_URL)/$(MPC_SIG)
# see http://www.multiprecision.org/index.php?prog=mpc&page=download
MPC_KEY      = AD17A21EF8AED8F1CC02DBD9F7D5C9BF765C61E3


#
# Interface to top-level prepare Makefile
#
PORTS += $(MPC)

MPC_INCLUDES = include/mpc/mpc.h

prepare-mpc: $(CONTRIB_DIR)/$(MPC) $(MPC_INCLUDES)

$(CONTRIB_DIR)/$(MPC): clean-mpc

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(MPC_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(MPC_URL) && touch $@

$(DOWNLOAD_DIR)/$(MPC_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(MPC_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(MPC_TGZ).verified: $(DOWNLOAD_DIR)/$(MPC_TGZ) \
                                     $(DOWNLOAD_DIR)/$(MPC_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(MPC_TGZ) $(DOWNLOAD_DIR)/$(MPC_SIG) $(MPC_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(MPC): $(DOWNLOAD_DIR)/$(MPC_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@

include/mpc/mpc.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../$(CONTRIB_DIR)/$(MPC)/src/mpc.h $@

clean-mpc:
	$(VERBOSE)rm -f  $(MPC_INCLUDES)
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(MPC)
