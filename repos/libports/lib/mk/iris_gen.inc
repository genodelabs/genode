LIBS = libc

include $(REP_DIR)/lib/mk/mesa-common.inc

INC_DIR += $(MESA_GEN_DIR)/src/compiler/nir \
           $(MESA_GEN_DIR)/src/intel \
           $(MESA_SRC_DIR)/src/compiler/nir \
           $(MESA_SRC_DIR)/src/gallium/auxiliary \
           $(MESA_SRC_DIR)/src/intel \
           $(MESA_SRC_DIR)/src/mapi \
           $(MESA_SRC_DIR)/src/mesa

CC_OPT  += -DGALLIUM_IRIS

CC_C_OPT += -Wno-unused-function

SRC_C = gallium/drivers/iris/iris_query.c \
        gallium/drivers/iris/iris_state.c \
        gallium/drivers/iris/iris_blorp.c 

vpath %.c $(MESA_SRC_DIR)/src
