TARGET   = rpi_platform_drv
REQUIRES = arm_v6
SRC_CC   = device.cc
SRC_CC  += device_component.cc
SRC_CC  += device_model_policy.cc
SRC_CC  += main.cc
SRC_CC  += session_component.cc
SRC_CC  += root.cc
INC_DIR  = $(PRG_DIR) $(REP_DIR)/src/drivers/platform
LIBS     = base

vpath main.cc $(PRG_DIR)
vpath %.cc    $(REP_DIR)/src/drivers/platform
