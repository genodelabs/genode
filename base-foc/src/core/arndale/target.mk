# override default location of thread context area within core
vpath thread_context_area.cc $(REP_DIR)/src/base/thread/arndale

include $(PRG_DIR)/../target.inc

LD_TEXT_ADDR = 0x80100000

REQUIRES += arm foc_arndale
SRC_CC   += arm/platform_arm.cc
INC_DIR  += $(REP_DIR)/src/core/include/arm

vpath platform_services.cc $(GEN_CORE_DIR)
