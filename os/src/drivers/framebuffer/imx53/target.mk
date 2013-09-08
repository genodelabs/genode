TARGET   = fb_drv
REQUIRES = imx53
SRC_CC   = main.cc
LIBS     = base blit config
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)
