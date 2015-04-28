include $(PRG_DIR)/../target.inc

LD_TEXT_ADDR = 0x80100000

REQUIRES += arm foc_odroid-x2
SRC_CC   += arm/platform_arm.cc
INC_DIR  += $(REP_DIR)/src/core/include/arm

vpath platform_services.cc $(GEN_CORE_DIR)
