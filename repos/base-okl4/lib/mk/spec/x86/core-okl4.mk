include $(REP_DIR)/lib/mk/core-okl4.inc

SRC_CC   += io_port_session_component.cc \
            io_port_session_support.cc \
            platform_thread_x86.cc

vpath io_port_session_component.cc $(GEN_CORE_DIR)/spec/x86
vpath io_port_session_support.cc   $(GEN_CORE_DIR)/spec/x86
vpath platform_services.cc         $(GEN_CORE_DIR)/spec/x86
vpath platform_thread_x86.cc       $(REP_DIR)/src/core/spec/x86

vpath crt0.s $(dir $(call select_from_repositories,src/lib/startup/spec/x86_32/crt0.s))
