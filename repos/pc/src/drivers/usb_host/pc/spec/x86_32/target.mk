
include $(REP_DIR)/src/drivers/usb_host/pc/target.inc

REQUIRES += 32bit
SRC_C    += lx_emul/spec/x86_32/atomic64_32.c
