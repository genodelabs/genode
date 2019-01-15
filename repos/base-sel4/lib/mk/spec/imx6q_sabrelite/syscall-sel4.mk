PLAT := imx6
ARCH := arm

SEL4_ARCH := aarch32
PLAT_BOARD := /imx6q_sabrelite
SEL4_WORDBITS := 32

include $(REP_DIR)/lib/mk/syscall-sel4.inc
