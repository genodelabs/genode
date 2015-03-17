include $(PRG_DIR)/../target.inc

LD_TEXT_ADDR = 0x80100000

REQUIRES += arm foc_arndale
SRC_CC   += arm/platform_arm.cc

vpath platform_services.cc $(GEN_CORE_DIR)
