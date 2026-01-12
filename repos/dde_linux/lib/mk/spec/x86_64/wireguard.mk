#
# For documentation see $(REP_DIR)/lib/mk/wireguard.inc
#

PRG_TOP_DIR := $(REP_DIR)/src/app/wireguard
PRG_DIR     := $(PRG_TOP_DIR)/spec/x86_64

SRC_S += lib/crypto/x86/poly1305-x86_64-cryptogams.S

INC_DIR += $(LX_SRC_DIR)/lib/crypto/x86

lib/crypto/x86/poly1305-x86_64-cryptogams.S:
	$(MSG_CONVERT)$@
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)perl $(LX_SRC_DIR)/lib/crypto/x86/poly1305-x86_64-cryptogams.pl > $@

include $(REP_DIR)/lib/mk/wireguard.inc
