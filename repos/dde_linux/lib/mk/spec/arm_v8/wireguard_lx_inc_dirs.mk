#
# For documentation see $(REP_DIR)/lib/mk/wireguard_lx_inc_dirs.inc .
#

PRG_DIR := $(REP_DIR)/src/app/wireguard/spec/arm_v8
GEN_PRG_DIR := $(PRG_DIR)/../..

DDE_LINUX_DIR := $(subst /src/include/lx_kit,,$(call select_from_repositories,src/include/lx_kit))

SRC_C += arch/arm64/kernel/smp.c
SRC_C += arch/arm64/kernel/cpufeature.c

vpath arch/arm64/kernel/cpufeature.c $(GEN_PRG_DIR)/lx_emul/shadow
vpath arch/arm64/kernel/smp.c        $(DDE_LINUX_DIR)/src/lib/lx_emul/shadow

SRC_S += arch/arm64/crypto/poly1305-core.S

arch/arm64/crypto/poly1305-core.S:
	$(MSG_CONVERT)$@
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)perl $(LX_SRC_DIR)/arch/arm64/crypto/poly1305-armv8.pl > $@

CC_OPT_arch/arm64/crypto/poly1305-core += -Dpoly1305_init=poly1305_init_arm64

include $(REP_DIR)/lib/mk/wireguard_lx_inc_dirs.inc

