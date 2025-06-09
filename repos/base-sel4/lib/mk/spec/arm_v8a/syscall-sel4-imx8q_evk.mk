PLAT := imx8mq-evk
ARCH := arm
BOARD := imx8q_evk

SEL4_ARCH := aarch64
SEL4_WORDBITS := 64

LIBS += kernel-sel4-imx8q_evk

include $(REP_DIR)/lib/mk/syscall-sel4.inc
