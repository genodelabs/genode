include $(PRG_DIR)/../target.inc

REQUIRES += x86

SRC_CC   += io_port_session_component.cc \
            platform_thread_x86.cc

vpath io_port_session_component.cc $(GEN_CORE_DIR)/x86
vpath platform_services.cc         $(GEN_CORE_DIR)/x86
vpath platform_thread_x86.cc       $(GEN_CORE_DIR)/x86
