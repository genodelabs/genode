#
# x86 real-mode emulation library
#

X86EMU_DIR  = $(call select_from_ports,x86emu)/src/lib/x86emu/contrib
INC_DIR    += $(X86EMU_DIR) $(REP_DIR)/include/x86emu
CC_OPT     += -fomit-frame-pointer -Wno-maybe-uninitialized

SRC_C = decode.c fpu.c ops.c ops2.c prim_ops.c sys.c

vpath %.c $(X86EMU_DIR)

CC_CXX_WARN_STRICT =
