include $(REP_DIR)/lib/mk/core.inc

REQUIRES += x86
SRC_CC   += platform_x86.cc

vpath io_port_session_component.cc $(GEN_CORE_DIR)/spec/x86
vpath io_port_session_support.cc   $(GEN_CORE_DIR)/spec/x86
vpath platform_services.cc         $(GEN_CORE_DIR)/spec/x86
vpath platform_x86.cc              $(REP_DIR)/src/core/spec/x86
