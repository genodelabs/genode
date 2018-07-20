
LIB_DIR = $(REP_DIR)/src/lib/zircon
ZIRCON_SYSTEM = $(call select_from_ports,dde_zircon)/zircon/src/system
ZIRCON_KERNEL = $(call select_from_ports,dde_zircon)/zircon/src/kernel
ZIRCON_THIRD_PARTY = $(call select_from_ports,dde_zircon)/zircon/src/third_party

INC_DIR += $(LIB_DIR)/pre_include \
	   $(ZIRCON_SYSTEM)/ulib/ddk/include \
	   $(ZIRCON_SYSTEM)/ulib/hid/include \
	   $(ZIRCON_SYSTEM)/public \
	   $(ZIRCON_KERNEL)/lib/libc/include \
	   $(ZIRCON_KERNEL)/lib/io/include \
	   $(ZIRCON_KERNEL)/lib/heap/include \
	   $(ZIRCON_KERNEL)/arch/x86/include \
	   $(ZIRCON_THIRD_PARTY)/ulib/musl/include \
	   $(LIB_DIR)/include

CC_C_OPT += -D_ALL_SOURCE -Wno-multichar
CC_CXX_OPT += -Wno-multichar
