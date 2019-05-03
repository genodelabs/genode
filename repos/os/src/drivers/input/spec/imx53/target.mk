TARGET    = imx53_input_drv
REQUIRES  = arm_v7
SRC_CC    = main.cc
LIBS      = base
INC_DIR  += $(PRG_DIR)
INC_DIR  += $(call select_from_repositories,include/spec/imx53)
