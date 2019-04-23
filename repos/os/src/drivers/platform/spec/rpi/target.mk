TARGET   = rpi_platform_drv
REQUIRES = arm_v6
SRC_CC   = main.cc
INC_DIR += ${PRG_DIR} $(call select_from_repositories,include/spec/rpi)
LIBS     = base

# enable C++11 support
CC_CXX_OPT += -std=gnu++11
