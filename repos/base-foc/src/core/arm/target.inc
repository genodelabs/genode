REQUIRES += arm
SRC_CC   += arm/platform_arm.cc \
            arm/ipc_pager.cc

include $(REP_DIR)/src/core/target.inc

vpath platform_services.cc $(GEN_CORE_DIR)
