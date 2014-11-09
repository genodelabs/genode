TARGET   = fb_drv
REQUIRES = platform_rpi
SRC_CC   = main.cc
LIBS     = base blit config
INC_DIR += $(PRG_DIR)

# enable C++11 support
CC_CXX_OPT += -std=gnu++11
