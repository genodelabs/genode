REQUIRES += arm
SRC_CC   += spec/arm/platform_arm.cc \
            spec/arm/ipc_pager.cc \
            platform_services.cc

# override default stack-area location
INC_DIR  += $(REP_DIR)/src/include/spec/arm

include $(REP_DIR)/lib/mk/core-foc.inc

vpath platform_services.cc $(GEN_CORE_DIR)
