TARGET   = imx8mq_platform_drv
REQUIRES = arm_v8
SRC_CC   = ccm.cc
SRC_CC  += device.cc
SRC_CC  += device_component.cc
SRC_CC  += device_model_policy.cc
SRC_CC  += imx_device.cc
SRC_CC  += main.cc
SRC_CC  += session_component.cc
SRC_CC  += root.cc
INC_DIR  = $(PRG_DIR) $(REP_DIR)/src/drivers/platform/spec/arm
LIBS     = base

vpath %.cc $(PRG_DIR)
vpath %.cc $(REP_DIR)/src/drivers/platform/spec/arm
