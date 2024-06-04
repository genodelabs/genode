#
# For documentation see $(REP_DIR)/lib/mk/wireguard.inc
#

PRG_TOP_DIR := $(REP_DIR)/src/app/wireguard
PRG_DIR     := $(PRG_TOP_DIR)/spec/arm_v8

DDE_LINUX_DIR := $(subst /src/include/lx_kit,,$(call select_from_repositories,src/include/lx_kit))

SRC_C += arch/arm64/kernel/smp.c
SRC_C += arch/arm64/kernel/cpufeature.c

vpath arch/arm64/kernel/cpufeature.c $(PRG_TOP_DIR)/lx_emul/shadow
vpath arch/arm64/kernel/smp.c        $(DDE_LINUX_DIR)/src/lib/lx_emul/shadow

SRC_S += arch/arm64/crypto/poly1305-core.S

arch/arm64/crypto/poly1305-core.S:
	$(MSG_CONVERT)$@
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)perl $(LX_SRC_DIR)/arch/arm64/crypto/poly1305-armv8.pl > $@

CC_OPT_arch/arm64/crypto/poly1305-core += -Dpoly1305_init=poly1305_init_arm64

include $(REP_DIR)/lib/mk/wireguard.inc
