SHARED_LIB = yes
LIBS       = libc

include $(REP_DIR)/lib/mk/mesa-common.inc

INC_DIR += $(MESA_GEN_DIR)/src/mapi \
           $(MESA_SRC_DIR)/src/mapi \
           $(MESA_SRC_DIR)/src/mapi/shared-glapi

SRC_C = mapi/entry.c \
        mapi/shared-glapi/glapi.c \
        mapi/shared-glapi/stub.c \
        mapi/shared-glapi/table.c \
        mapi/u_current.c \

CC_OPT += -DMAPI_ABI_HEADER=\"shared-glapi/glapi_mapi_tmp.h\" -DMAPI_MODE_GLAPI

vpath %.c $(MESA_SRC_DIR)/src
