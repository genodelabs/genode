SRC_CC += $(addprefix spec/x86_64/, boot_info.cc thread.cc platform.cc \
                                    platform_pd.cc vm_space.cc)

SRC_CC += io_port_session_component.cc
SRC_CC += io_port_session_support.cc

include $(REP_DIR)/lib/mk/core-sel4.inc

vpath io_port_session_component.cc $(GEN_CORE_DIR)/spec/x86
vpath platform_services.cc         $(GEN_CORE_DIR)/spec/x86
