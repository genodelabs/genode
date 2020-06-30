PLAT  := imx6
CPU   := cortex-a9
override BOARD := imx6q_sabrelite

-include $(REP_DIR)/lib/mk/spec/arm/kernel-sel4.inc
