include $(REP_DIR)/src/core/target.inc

REQUIRES += x86
SRC_CC   += io_port_session_component.cc \
            platform_x86.cc

vpath io_port_session_component.cc $(GEN_CORE_DIR)/spec/x86
vpath platform_services.cc         $(GEN_CORE_DIR)/spec/x86

