PLAT := imx6
ARCH := arm
BOARD := imx6q_sabrelite

SEL4_ARCH := aarch32
SEL4_WORDBITS := 32

LIBS += kernel-sel4-imx6q_sabrelite

include $(REP_DIR)/lib/mk/syscall-sel4.inc
