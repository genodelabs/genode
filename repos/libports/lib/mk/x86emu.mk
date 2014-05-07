#
# x86 real-mode emulation library
#

X86EMU      = x86emu-1.12.0
X86EMU_DIR  = $(REP_DIR)/contrib/$(X86EMU)
INC_DIR    += $(X86EMU_DIR) $(REP_DIR)/include/x86emu
CC_OPT     += -fomit-frame-pointer

SRC_C = decode.c fpu.c ops.c ops2.c prim_ops.c sys.c

vpath %.c $(X86EMU_DIR)
