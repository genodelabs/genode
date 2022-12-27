TARGET = triangle_gl
LIBS   = base libm libc egl mesa

SRC_C  = eglut.c main.c
SRC_CC = eglut_genode.cc
LD_OPT = --export-dynamic

EGLUT_DIR = $(PRG_DIR)/../eglut

INC_DIR += $(REP_DIR)/src/lib/mesa/include \
           $(EGLUT_DIR)

vpath %.c  $(EGLUT_DIR)
vpath %.cc $(EGLUT_DIR)


CC_CXX_WARN_STRICT =
