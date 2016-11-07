include $(PRG_DIR)/../../target.inc

REQUIRES += x86
SRC_CC   += platform_x86.cc

vpath io_port_session_component.cc $(GEN_CORE_DIR)/spec/x86
vpath io_port_session_support.cc   $(GEN_CORE_DIR)/spec/x86
vpath platform_services.cc         $(GEN_CORE_DIR)/spec/x86

