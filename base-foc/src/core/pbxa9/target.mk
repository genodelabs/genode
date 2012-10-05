include $(PRG_DIR)/../target.inc

REQUIRES += arm foc_pbxa9
SRC_CC   += arm/platform_arm.cc
INC_DIR  += $(REP_DIR)/src/core/include/arm

LD_TEXT_ADDR = 0x70490000

vpath io_port_session_component.cc $(GEN_CORE_DIR)/arm

