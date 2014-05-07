TARGET  = test-libpng_static
SRC_CC  = main.cc
INC_DIR = $(REP_DIR)/include/libpng \
          $(REP_DIR)/include/mini_c \
          $(REP_DIR)/include/libz
CC_OPT += -DPNG_USER_CONFIG
LIBS    = base libpng_static libz_static mini_c
