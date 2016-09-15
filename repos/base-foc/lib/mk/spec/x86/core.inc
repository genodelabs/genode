REQUIRES    += x86
SRC_CC      += io_port_session_component.cc \
               io_port_session_support.cc \
               spec/x86/ipc_pager.cc \
               spec/x86/platform.cc

vpath io_port_session_component.cc $(BASE_DIR)/src/core/spec/x86
vpath io_port_session_support.cc   $(BASE_DIR)/src/core/spec/x86
vpath platform_services.cc         $(BASE_DIR)/src/core/spec/x86

include $(REP_DIR)/lib/mk/core.inc
