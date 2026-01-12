#
# For documentation see $(REP_DIR)/lib/mk/wireguard.inc
#

PRG_TOP_DIR := $(REP_DIR)/src/app/wireguard
PRG_DIR     := $(PRG_TOP_DIR)/spec/arm_v8

DDE_LINUX_DIR := $(subst /src/include/lx_kit,,$(call select_from_repositories,src/include/lx_kit))

SRC_C += arch/arm64/kernel/smp.c

vpath arch/arm64/kernel/smp.c        $(DDE_LINUX_DIR)/src/lib/lx_emul/shadow

SRC_S += lib/crypto/arm64/poly1305-core.S

INC_DIR += $(LX_SRC_DIR)/lib/crypto/arm64

lib/crypto/arm64/poly1305-core.S:
	$(MSG_CONVERT)$@
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)perl $(LX_SRC_DIR)/lib/crypto/arm64/poly1305-armv8.pl > $@

include $(REP_DIR)/lib/mk/wireguard.inc
