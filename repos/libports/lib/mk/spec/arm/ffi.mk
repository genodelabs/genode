SHARED_LIB = yes

LIBS = libc

FFI_PORT = $(call select_from_ports,ffi)/src/lib/ffi

INC_DIR = $(FFI_PORT)/include

INC_DIR += $(call select_from_ports,ffi)/include/ffi/arm \
           $(REP_DIR)/src/lib/ffi

CC_OPT = -DFFI_NO_RAW_API=0

SRC_C = prep_cif.c types.c ffi.c
SRC_S = sysv.S

vpath prep_cif.c $(FFI_PORT)/src
vpath types.c    $(FFI_PORT)/src
vpath ffi.c    $(FFI_PORT)/src/arm
vpath sysv.S   $(FFI_PORT)/src/arm
