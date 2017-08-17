SHARED_LIB = yes
LIBS       = libc

include $(REP_DIR)/lib/mk/mesa-common.inc

SRC_C = mapi/entry.c \
        mapi/mapi_glapi.c \
        mapi/stub.c \
        mapi/table.c \
        mapi/u_current.c \
        mapi/u_execmem.c

CC_OPT += -DMAPI_ABI_HEADER=\"shared-glapi/glapi_mapi_tmp.h\" -DMAPI_MODE_GLAPI

vpath %.c $(MESA_PORT_DIR)/src

