#
# For documentation see $(REP_DIR)/lib/mk/wireguard_lx_inc_dirs.inc .
#

PRG_DIR := $(REP_DIR)/src/app/wireguard/spec/x86_64

SRC_S += arch/x86/crypto/poly1305-x86_64-cryptogams.S

arch/x86/crypto/poly1305-x86_64-cryptogams.S:
	$(MSG_CONVERT)$@
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)perl $(LX_SRC_DIR)/arch/x86/crypto/poly1305-x86_64-cryptogams.pl > $@

include $(REP_DIR)/lib/mk/wireguard_lx_inc_dirs.inc
