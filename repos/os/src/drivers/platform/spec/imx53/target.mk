TARGET   = imx53_platform_drv
REQUIRES = arm_v7
SRC_CC   = main.cc
INC_DIR += ${PRG_DIR} $(call select_from_repositories,include/spec/imx53)
LIBS     = base
