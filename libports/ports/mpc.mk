include lib/import/import-mpc.mk

MPC_TGZ = $(MPC).tar.gz
MPC_URL = http://www.multiprecision.org/mpc/download/$(MPC_TGZ)

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

$(CONTRIB_DIR)/$(MPC): $(DOWNLOAD_DIR)/$(MPC_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

include/mpc/mpc.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../$(CONTRIB_DIR)/$(MPC)/src/mpc.h $@

clean-mpc:
	$(VERBOSE)rm -f  $(MPC_INCLUDES)
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(MPC)
