PLAT  := imx8mq-evk
CPU   := cortex-a9
override BOARD := imx8q_evk
ARM_PLATFORM := imx8mq-evk

-include $(REP_DIR)/lib/mk/spec/arm_v8a/kernel-sel4.inc
