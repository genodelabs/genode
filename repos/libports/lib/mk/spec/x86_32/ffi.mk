SHARED_LIB = yes

LIBS = libc

FFI_PORT = $(call select_from_ports,ffi)/src/lib/ffi

INC_DIR = $(FFI_PORT)/include

INC_DIR += $(call select_from_ports,ffi)/include/ffi/spec/x86_32 \
           $(REP_DIR)/src/lib/ffi

CC_OPT = -DFFI_NO_RAW_API=0

SRC_C = prep_cif.c types.c ffi.c dummies.c
SRC_S = sysv.S

CC_OPT_sysv = -DHAVE_AS_ASCII_PSEUDO_OP=1 -DHAVE_AS_X86_PCREL=1

vpath prep_cif.c $(FFI_PORT)/src
vpath types.c    $(FFI_PORT)/src
vpath ffi.c      $(FFI_PORT)/src/x86
vpath sysv.S     $(FFI_PORT)/src/x86
vpath dummies.c  $(REP_DIR)/src/lib/ffi
