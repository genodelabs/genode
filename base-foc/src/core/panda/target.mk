include $(PRG_DIR)/../target.inc

LD_TEXT_ADDR = 0x80140000

REQUIRES += arm foc_panda
SRC_CC   += arm/platform_arm.cc
INC_DIR  += $(REP_DIR)/src/core/include/arm


vpath io_port_session_component.cc $(GEN_CORE_DIR)/arm

