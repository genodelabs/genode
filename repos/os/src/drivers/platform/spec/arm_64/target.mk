TARGET   = platform_drv
REQUIRES = arm_64
SRC_CC   = device.cc device_component.cc device_model_policy.cc main.cc session_component.cc root.cc
INC_DIR  = $(PRG_DIR)/../../spec/arm
LIBS     = base

vpath %.cc $(PRG_DIR)/../../spec/arm
