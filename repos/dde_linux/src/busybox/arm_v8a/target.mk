$(call check_tool,aarch64-linux-gnu-gcc)

REQUIRES   := arm_v8a
BB_MK_ARGS := ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

include $(PRG_DIR)/../target.inc
