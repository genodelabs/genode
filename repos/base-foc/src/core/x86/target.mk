include $(PRG_DIR)/../target.inc

REQUIRES += x86

SRC_CC   += io_port_session_component.cc \
            x86/platform_x86.cc

INC_DIR  += $(REP_DIR)/src/core/include/x86

vpath io_port_session_component.cc $(GEN_CORE_DIR)/x86
vpath platform_services.cc         $(GEN_CORE_DIR)/x86
