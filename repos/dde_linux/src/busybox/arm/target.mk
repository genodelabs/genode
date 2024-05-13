$(call check_tool,arm-linux-gnueabihf-gcc)

REQUIRES   := arm
BB_MK_ARGS := ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

include $(PRG_DIR)/../target.inc
