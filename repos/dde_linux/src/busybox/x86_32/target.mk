$(call check_tool,i686-linux-gnu-gcc)

REQUIRES   := x86_32
BB_MK_ARGS := ARCH=i386 CROSS_COMPILE=i686-linux-gnu-

include $(PRG_DIR)/../target.inc
