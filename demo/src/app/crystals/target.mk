TARGET   = crystals
SRC_CC   = main.cc
LIBS     = base nano3d libpng_static libz_static mini_c
CC_OPT  += -DPNG_USER_CONFIG
INC_DIR += $(PRG_DIR)
