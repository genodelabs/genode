TARGET   = imx53_fb_drv
REQUIRES = arm_v7
SRC_CC   = main.cc
LIBS     = base blit
INC_DIR += $(PRG_DIR)
INC_DIR += $(call select_from_repositories,include/spec/imx53)

CC_CXX_WARN_STRICT_CONVERSION =
