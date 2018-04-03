SHARED_LIB = yes

LIBS = libc

FFI_PORT = $(call select_from_ports,ffi)/src/lib/ffi

INC_DIR = $(FFI_PORT)/include

INC_DIR += $(call select_from_ports,ffi)/include/ffi/x86_64 \
           $(REP_DIR)/src/lib/ffi

CC_OPT = -DFFI_NATAIVE_RAW_API=1 -DFFI_NO_RAW_API=0

SRC_C = prep_cif.c types.c ffi64.c
SRC_S = unix64.S

CC_OPT_unix64 = -DHAVE_AS_X86_PCREL=1

vpath prep_cif.c $(FFI_PORT)/src
vpath types.c    $(FFI_PORT)/src
vpath ffi64.c    $(FFI_PORT)/src/x86
vpath unix64.S   $(FFI_PORT)/src/x86
