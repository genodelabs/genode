PLAT := imx7
ARCH := arm

SEL4_ARCH := aarch32
PLAT_BOARD := /imx7d_sabre
SEL4_WORDBITS := 32

include $(REP_DIR)/lib/mk/syscall-sel4.inc
