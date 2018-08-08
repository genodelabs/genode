REQUIRES = x86

TARGET = zx_pc_ps2_drv
LIBS = base zircon

SRC_CC += main.cc

SRC_C += i8042.c

ZIRCON_SYSTEM = $(call select_from_ports,dde_zircon)/zircon/src/system

INC_DIR += $(PRG_DIR)/include

vpath %.cc $(PRG_DIR)
vpath i8042.c $(ZIRCON_SYSTEM)/dev/input/pc-ps2
