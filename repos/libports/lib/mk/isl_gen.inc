LIBS = libc

include $(REP_DIR)/lib/mk/mesa-common.inc

INC_DIR += $(MESA_GEN_DIR)/src/intel \
           $(MESA_SRC_DIR)/src/intel

SRC_C = intel/isl/isl_emit_cpb.c \
        intel/isl/isl_emit_depth_stencil.c \
        intel/isl/isl_surface_state.c

vpath %.c $(MESA_SRC_DIR)/src
