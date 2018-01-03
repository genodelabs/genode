TARGET = gears
LIBS   = libm libc egl mesa

SRC_C     = eglgears.c eglut.c
SRC_CC    = eglut_genode.cc
LD_OPT    = --export-dynamic

INC_DIR  += $(REP_DIR)/src/lib/mesa/include \
            $(PRG_DIR)/../eglut

vpath %.c  $(PRG_DIR)/../eglut
vpath %.cc $(PRG_DIR)/../eglut

CC_CXX_WARN_STRICT =
