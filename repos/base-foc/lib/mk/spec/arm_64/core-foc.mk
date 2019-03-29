REQUIRES += arm_64
SRC_CC   += spec/arm/platform_arm.cc \
            spec/arm_64/ipc_pager.cc \
            platform_services.cc

include $(REP_DIR)/lib/mk/core-foc.inc

vpath platform_services.cc $(GEN_CORE_DIR)
