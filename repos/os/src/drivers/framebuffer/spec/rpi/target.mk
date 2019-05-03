TARGET   = rpi_fb_drv
REQUIRES = arm_v6
SRC_CC   = main.cc
LIBS     = base blit
INC_DIR += $(PRG_DIR)
INC_DIR += $(call select_from_repositories,include/spec/rpi)
