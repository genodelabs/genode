REQUIRES += arm
SRC_CC   += spec/arm/platform_arm.cc \
            spec/arm/ipc_pager.cc

include $(REP_DIR)/src/core/target.inc

vpath platform_services.cc $(GEN_CORE_DIR)
