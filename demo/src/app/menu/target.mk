TARGET   = menu
SRC_CC   = main.cc
SRC_BIN  = hover.raw select.raw
LIBS     = base nano3d libpng_static libz_static mini_c config
CC_OPT  += -DPNG_USER_CONFIG
INC_DIR += $(PRG_DIR)
