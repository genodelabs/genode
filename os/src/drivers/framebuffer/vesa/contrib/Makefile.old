PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= libx86emu.a
MODE		= sigma0
SYSTEMS		= x86-l4v2 amd64-l4v2

SRC_C		= decode.c fpu.c ops.c ops2.c prim_ops.c sys.c
CPPFLAGS	= -DNO_LONG_LONG
CFLAGS		= -fomit-frame-pointer
PRIVATE_INCDIR	= $(PKGDIR)/lib/src
WARNINGS	= -Wall -Wstrict-prototypes

DEBUG 		= 0
ifeq ($(DEBUG),1)
SRC_C		+= debug.c
endif

include $(L4DIR)/mk/lib.mk
